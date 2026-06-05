
#!/usr/bin/env python3
"""
PiNTY Flash file generator

Desktop utility for Raspberry Pi RP2040 / RP2350 workflows:
1. Build a LittleFS image from a selectable folder using mklittlefs
2. Pad a selected Firmware Binary (.bin file) to a selectable size and append the LittleFS image
3. Convert the merged .bin file into a .uf2 file ready for flashing

Simplified LittleFS options exposed in the GUI:
- fs image size
- fs block size
- fs page size

Information-only UF2 fields (not editable):
- flash address
- UF2 family ID
- UF2 payload size

Presets:
- RP2350 (default)
- RP2040
"""

from __future__ import annotations

import os
import queue
import shutil
import struct
import subprocess
import sys
import threading
from dataclasses import dataclass
from pathlib import Path
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

APP_TITLE = "PiNTY Flash file generator"

TARGET_CONFIGS = {
    "RP2350": {
        "family_id": 0xE48BFF5A,
        "flash_address": 0x10000000,
        "uf2_payload_size": 256,
        "block_size": "4096",
        "page_size": "256",
        "image_size": "0x100000",
        "pad_size": "0x100000",
    },
    "RP2040": {
        "family_id": 0xE48BFF56,
        "flash_address": 0x10000000,
        "uf2_payload_size": 256,
        "block_size": "4096",
        "page_size": "256",
        "image_size": "0x100000",
        "pad_size": "0x100000",
    },
}

DEFAULTS = {
    "target": "RP2350",
    "pad_value": "0xFF",
    "fs_name": "littlefs.bin",
    "merged_name": "merged.bin",
    "uf2_name": "merged.uf2",
}

UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_FAMILY_ID_PRESENT = 0x00002000
UF2_DATA_BYTES_PER_BLOCK = 476


@dataclass
class BuildConfig:
    target: str
    family_id: int
    flash_address: int
    folder: Path
    firmware_binary: Path
    output_dir: Path
    mklittlefs_path: str
    fs_image: Path
    merged_bin: Path
    uf2_file: Path
    image_size: int
    block_size: int
    page_size: int
    firmware_pad_size: int
    pad_value: int
    uf2_payload_size: int


def parse_int(text: str, name: str) -> int:
    text = text.strip()
    if not text:
        raise ValueError(f"Missing {name}.")
    try:
        return int(text, 0)
    except ValueError as exc:
        raise ValueError(f"Invalid {name}: {text!r}") from exc


def human_size(n: int) -> str:
    units = ["B", "KB", "MB", "GB"]
    value = float(n)
    for unit in units:
        if value < 1024 or unit == units[-1]:
            return f"{int(value)} {unit}" if unit == "B" else f"{value:.2f} {unit}"
        value /= 1024
    return f"{n} B"


def quote_arg(arg: str) -> str:
    return f'"{arg}"' if any(ch.isspace() for ch in arg) else arg


def indent(text: str, prefix: str = "    ") -> str:
    return "\n".join(prefix + line for line in text.splitlines())


def compute_dir_size(root: Path) -> int:
    return sum(p.stat().st_size for p in root.rglob("*") if p.is_file())


def auto_find_mklittlefs() -> str | None:
    exe = "mklittlefs.exe" if os.name == "nt" else "mklittlefs"
    try:
        base = Path(__file__).resolve().parent
        candidates = [base / exe, base / "tools" / exe]
        for candidate in candidates:
            if candidate.exists():
                return str(candidate.resolve())
    except Exception:
        pass
    return shutil.which(exe) or shutil.which("mklittlefs")


class TextLogger:
    def __init__(self, widget: tk.Text):
        self.widget = widget
        self.widget.configure(state="disabled")

    def write(self, text: str) -> None:
        self.widget.configure(state="normal")
        self.widget.insert("end", text)
        self.widget.see("end")
        self.widget.configure(state="disabled")
        self.widget.update_idletasks()

    def clear(self) -> None:
        self.widget.configure(state="normal")
        self.widget.delete("1.0", "end")
        self.widget.configure(state="disabled")


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title(APP_TITLE)
        self.geometry("980x730")
        self.minsize(920, 660)
        self.msg_queue: queue.Queue[tuple[str, str]] = queue.Queue()
        self.worker: threading.Thread | None = None

        self._vars()
        self._ui()
        self._apply_target_preset(force_all=True)
        self.after(100, self._drain)

    def _vars(self) -> None:
        self.var_target = tk.StringVar(value=DEFAULTS["target"])
        self.var_folder = tk.StringVar()
        self.var_firmware_binary = tk.StringVar()
        self.var_output_dir = tk.StringVar()
        self.var_mklittlefs = tk.StringVar(value=auto_find_mklittlefs() or "")

        self.var_image_size = tk.StringVar()
        self.var_block_size = tk.StringVar()
        self.var_page_size = tk.StringVar()

        self.var_pad_size = tk.StringVar()
        self.var_pad_value = tk.StringVar(value=DEFAULTS["pad_value"])
        self.var_flash_address = tk.StringVar()
        self.var_family_id = tk.StringVar()
        self.var_uf2_payload_size = tk.StringVar()

        self.var_fs_name = tk.StringVar(value=DEFAULTS["fs_name"])
        self.var_merged_name = tk.StringVar(value=DEFAULTS["merged_name"])
        self.var_uf2_name = tk.StringVar(value=DEFAULTS["uf2_name"])

    def _ui(self) -> None:
        root = ttk.Frame(self, padding=10)
        root.pack(fill="both", expand=True)

        ttk.Label(root, text=APP_TITLE, font=("Segoe UI", 16, "bold")).pack(anchor="w", pady=(0, 10))

        lf_paths = ttk.LabelFrame(root, text="Inputs only", padding=10)
        lf_paths.pack(fill="x", pady=(0, 10))
        self._combo_row(lf_paths, 0, "Target MCU", self.var_target, list(TARGET_CONFIGS.keys()), self._on_target_changed)
        self._browse_row(lf_paths, 1, "Source folder", self.var_folder, self._pick_folder)
        self._browse_row(lf_paths, 2, "Firmware Binary", self.var_firmware_binary, self._pick_firmware_binary)
        self._browse_row(lf_paths, 3, "mklittlefs tool", self.var_mklittlefs, self._pick_tool)

        lf_names = ttk.LabelFrame(root, text="Generated Files", padding=10)
        lf_names.pack(fill="x", pady=(0, 10))
        self._browse_row(lf_names, 0, "Output directory", self.var_output_dir, self._pick_output)
        self._entry_row(lf_names, 1, "Filesystem image name", self.var_fs_name)
        self._entry_row(lf_names, 2, "Merged BIN name", self.var_merged_name)
        self._entry_row(lf_names, 3, "UF2 file name", self.var_uf2_name)

        middle = ttk.Frame(root)
        middle.pack(fill="x", pady=(0, 10))

        lf_lfs = ttk.LabelFrame(middle, text="LittleFS settings", padding=10)
        lf_lfs.pack(side="left", fill="both", expand=True, padx=(0, 5))
        self._entry_row(lf_lfs, 0, "Image size", self.var_image_size)
        self._entry_row(lf_lfs, 1, "Block size", self.var_block_size)
        self._entry_row(lf_lfs, 2, "Page size", self.var_page_size)

        lf_uf2 = ttk.LabelFrame(middle, text="Merge / UF2 settings", padding=10)
        lf_uf2.pack(side="left", fill="both", expand=True, padx=(5, 0))
        self._entry_row(lf_uf2, 0, "Pad Firmware Binary to size", self.var_pad_size)
        self._entry_row(lf_uf2, 1, "Pad value", self.var_pad_value)
        self._readonly_row(lf_uf2, 2, "Flash address", self.var_flash_address)
        self._readonly_row(lf_uf2, 3, "UF2 family ID", self.var_family_id)
        self._readonly_row(lf_uf2, 4, "UF2 payload size", self.var_uf2_payload_size)

        actions = ttk.Frame(root)
        actions.pack(fill="x", pady=(0, 10))
        self.btn_build = ttk.Button(actions, text="Build All", command=self._start_build)
        self.btn_build.pack(side="left")
        ttk.Button(actions, text="Preview Paths", command=self._preview_paths).pack(side="left", padx=(8, 0))
        ttk.Button(actions, text="Clear Log", command=lambda: self.logger.clear()).pack(side="left", padx=(8, 0))
        self.progress = ttk.Progressbar(actions, mode="indeterminate")
        self.progress.pack(side="right", fill="x", expand=True, padx=(10, 0))

        lf_log = ttk.LabelFrame(root, text="Log", padding=10)
        lf_log.pack(fill="both", expand=True)
        self.text_log = tk.Text(lf_log, wrap="word", height=20)
        self.text_log.pack(fill="both", expand=True)
        self.logger = TextLogger(self.text_log)
        self.logger.write(
            "Ready.\n"
            "Simplified workflow:\n"
            "  1) Build a LittleFS image from the selected folder\n"
            "  2) Pad the Firmware Binary to the selected size and append the filesystem image\n"
            "  3) Convert the merged BIN to UF2 for RP2350 or RP2040\n\n"
            "UF2-sensitive fields are shown for information only and follow the selected preset.\n\n"
        )

    def _entry_row(self, parent, row: int, label: str, var: tk.StringVar) -> None:
        ttk.Label(parent, text=label, width=24).grid(row=row, column=0, sticky="w", padx=(0, 10), pady=4)
        ttk.Entry(parent, textvariable=var).grid(row=row, column=1, sticky="ew", pady=4)
        parent.grid_columnconfigure(1, weight=1)

    def _readonly_row(self, parent, row: int, label: str, var: tk.StringVar) -> None:
        ttk.Label(parent, text=label, width=24).grid(row=row, column=0, sticky="w", padx=(0, 10), pady=4)
        entry = ttk.Entry(parent, textvariable=var, state="readonly")
        entry.grid(row=row, column=1, sticky="ew", pady=4)
        parent.grid_columnconfigure(1, weight=1)

    def _browse_row(self, parent, row: int, label: str, var: tk.StringVar, cb) -> None:
        ttk.Label(parent, text=label, width=24).grid(row=row, column=0, sticky="w", padx=(0, 10), pady=4)
        ttk.Entry(parent, textvariable=var).grid(row=row, column=1, sticky="ew", pady=4)
        ttk.Button(parent, text="Browse...", command=cb).grid(row=row, column=2, padx=(8, 0), pady=4)
        parent.grid_columnconfigure(1, weight=1)

    def _combo_row(self, parent, row: int, label: str, var: tk.StringVar, values, cb=None) -> None:
        ttk.Label(parent, text=label, width=24).grid(row=row, column=0, sticky="w", padx=(0, 10), pady=4)
        combo = ttk.Combobox(parent, textvariable=var, values=values, state="readonly")
        combo.grid(row=row, column=1, sticky="ew", pady=4)
        if cb:
            combo.bind("<<ComboboxSelected>>", cb)
        parent.grid_columnconfigure(1, weight=1)

    def _pick_folder(self) -> None:
        p = filedialog.askdirectory(title="Select source folder")
        if p:
            self.var_folder.set(p)
            if not self.var_output_dir.get():
                self.var_output_dir.set(p)

    def _pick_firmware_binary(self) -> None:
        p = filedialog.askopenfilename(title="Select Firmware Binary", filetypes=[("BIN files", "*.bin"), ("All files", "*.*")])
        if p:
            self.var_firmware_binary.set(p)
            if not self.var_output_dir.get():
                self.var_output_dir.set(str(Path(p).parent))

    def _pick_output(self) -> None:
        p = filedialog.askdirectory(title="Select output directory")
        if p:
            self.var_output_dir.set(p)

    def _pick_tool(self) -> None:
        p = filedialog.askopenfilename(title="Select mklittlefs", filetypes=[("All files", "*.*")])
        if p:
            self.var_mklittlefs.set(p)

    def _on_target_changed(self, event=None) -> None:
        self._apply_target_preset(force_all=False)

    def _apply_target_preset(self, force_all: bool) -> None:
        target = self.var_target.get().strip() or DEFAULTS["target"]
        preset = TARGET_CONFIGS.get(target, TARGET_CONFIGS[DEFAULTS["target"]])
        self.var_family_id.set(hex(preset["family_id"]))
        self.var_flash_address.set(hex(preset["flash_address"]))
        self.var_uf2_payload_size.set(str(preset["uf2_payload_size"]))
        if force_all or not self.var_block_size.get().strip():
            self.var_block_size.set(preset["block_size"])
        if force_all or not self.var_page_size.get().strip():
            self.var_page_size.set(preset["page_size"])
        if force_all or not self.var_image_size.get().strip():
            self.var_image_size.set(preset["image_size"])
        if force_all or not self.var_pad_size.get().strip():
            self.var_pad_size.set(preset["pad_size"])

    def _preview_paths(self) -> None:
        try:
            cfg = self._read_config()
        except Exception as exc:
            messagebox.showerror(APP_TITLE, str(exc))
            return
        self.logger.write(
            f"Planned outputs:\n"
            f"  Output directory   : {cfg.output_dir}\n"
            f"  Filesystem image   : {cfg.fs_image}\n"
            f"  Merged BIN         : {cfg.merged_bin}\n"
            f"  UF2                : {cfg.uf2_file}\n"
            f"  Target MCU         : {cfg.target}\n"
            f"  Flash address      : 0x{cfg.flash_address:08X}\n"
            f"  UF2 family ID      : 0x{cfg.family_id:08X}\n"
            f"  UF2 payload size   : {cfg.uf2_payload_size}\n\n"
        )

    def _read_config(self) -> BuildConfig:
        target = self.var_target.get().strip() or DEFAULTS["target"]
        if target not in TARGET_CONFIGS:
            raise ValueError(f"Unsupported target MCU: {target}")

        folder = Path(self.var_folder.get().strip())
        firmware_binary = Path(self.var_firmware_binary.get().strip())
        output_dir = Path(self.var_output_dir.get().strip())
        mklfs = self.var_mklittlefs.get().strip() or auto_find_mklittlefs() or ""

        if not folder.exists() or not folder.is_dir():
            raise ValueError("Please select a valid source folder.")
        if not firmware_binary.exists() or not firmware_binary.is_file():
            raise ValueError("Please select a valid Firmware Binary file.")
        if not output_dir:
            raise ValueError("Please select a valid output directory.")
        output_dir.mkdir(parents=True, exist_ok=True)
        if not mklfs:
            raise ValueError("Could not find mklittlefs. Select it manually or place it next to the script.")
        if not Path(mklfs).exists() and not shutil.which(mklfs):
            raise ValueError("mklittlefs not found at the selected location.")

        cfg = BuildConfig(
            target=target,
            family_id=parse_int(self.var_family_id.get(), "UF2 family ID"),
            flash_address=parse_int(self.var_flash_address.get(), "flash address"),
            folder=folder.resolve(),
            firmware_binary=firmware_binary.resolve(),
            output_dir=output_dir.resolve(),
            mklittlefs_path=str(Path(mklfs).resolve()) if Path(mklfs).exists() else mklfs,
            fs_image=(output_dir / (self.var_fs_name.get().strip() or DEFAULTS["fs_name"])).resolve(),
            merged_bin=(output_dir / (self.var_merged_name.get().strip() or DEFAULTS["merged_name"])).resolve(),
            uf2_file=(output_dir / (self.var_uf2_name.get().strip() or DEFAULTS["uf2_name"])).resolve(),
            image_size=parse_int(self.var_image_size.get(), "image size"),
            block_size=parse_int(self.var_block_size.get(), "block size"),
            page_size=parse_int(self.var_page_size.get(), "page size"),
            firmware_pad_size=parse_int(self.var_pad_size.get(), "pad size"),
            pad_value=parse_int(self.var_pad_value.get(), "pad value"),
            uf2_payload_size=parse_int(self.var_uf2_payload_size.get(), "UF2 payload size"),
        )

        if cfg.image_size <= 0:
            raise ValueError("Image size must be > 0.")
        if cfg.block_size <= 0:
            raise ValueError("Block size must be > 0.")
        if cfg.page_size <= 0:
            raise ValueError("Page size must be > 0.")
        if cfg.firmware_pad_size <= 0:
            raise ValueError("Pad size must be > 0.")
        if not (0 <= cfg.pad_value <= 0xFF):
            raise ValueError("Pad value must be between 0 and 255.")
        if cfg.page_size > cfg.block_size:
            raise ValueError("Page size cannot be larger than block size.")
        if cfg.block_size > cfg.image_size:
            raise ValueError("Block size cannot be larger than image size.")
        if cfg.firmware_binary.stat().st_size > cfg.firmware_pad_size:
            raise ValueError(
                f"Firmware Binary size ({cfg.firmware_binary.stat().st_size}) is larger than selected pad size ({cfg.firmware_pad_size})."
            )
        if cfg.uf2_payload_size <= 0 or cfg.uf2_payload_size > UF2_DATA_BYTES_PER_BLOCK:
            raise ValueError(f"UF2 payload size must be between 1 and {UF2_DATA_BYTES_PER_BLOCK} bytes.")

        return cfg

    def _set_enabled(self, enabled: bool) -> None:
        state = "normal" if enabled else "disabled"
        for child in self.winfo_children():
            self._set_state_recursive(child, state)
        self.text_log.configure(state="disabled")

    def _set_state_recursive(self, widget, state: str) -> None:
        try:
            widget.configure(state=state)
        except tk.TclError:
            pass
        for child in widget.winfo_children():
            self._set_state_recursive(child, state)

    def _start_build(self) -> None:
        if self.worker and self.worker.is_alive():
            messagebox.showinfo(APP_TITLE, "A build is already running.")
            return
        try:
            cfg = self._read_config()
        except Exception as exc:
            messagebox.showerror(APP_TITLE, str(exc))
            return

        self._set_enabled(False)
        self.progress.start(10)
        self.logger.write("Starting build...\n")
        self.worker = threading.Thread(target=self._worker, args=(cfg,), daemon=True)
        self.worker.start()

    def _worker(self, cfg: BuildConfig) -> None:
        try:
            self._post("log", f"Target MCU        : {cfg.target}\n")
            self._post("log", f"Source folder     : {cfg.folder}\n")
            self._post("log", f"Firmware Binary   : {cfg.firmware_binary}\n")
            self._post("log", f"Output directory  : {cfg.output_dir}\n")
            self._post("log", f"mklittlefs        : {cfg.mklittlefs_path}\n")
            self._post("log", f"Flash address     : 0x{cfg.flash_address:08X}\n")
            self._post("log", f"UF2 family ID     : 0x{cfg.family_id:08X}\n")
            self._post("log", f"UF2 payload size  : {cfg.uf2_payload_size}\n\n")
            build_littlefs_image(cfg, self._post_log)
            merge_firmware_and_fs(cfg, self._post_log)
            convert_merged_bin_to_uf2(cfg, self._post_log)
            self._post("log", f"SUCCESS\n  LittleFS image  : {cfg.fs_image}\n  Merged BIN      : {cfg.merged_bin}\n  UF2             : {cfg.uf2_file}\n")
        except Exception as exc:
            self._post("log", f"ERROR: {exc}\n")
        finally:
            self._post("done", "")

    def _post(self, kind: str, payload: str) -> None:
        self.msg_queue.put((kind, payload))

    def _post_log(self, payload: str) -> None:
        self._post("log", payload)

    def _drain(self) -> None:
        try:
            while True:
                kind, payload = self.msg_queue.get_nowait()
                if kind == "log":
                    self.logger.write(payload)
                elif kind == "done":
                    self.progress.stop()
                    self._set_enabled(True)
        except queue.Empty:
            pass
        self.after(100, self._drain)


def build_littlefs_image(cfg: BuildConfig, log) -> None:
    payload = compute_dir_size(cfg.folder)
    log("[1/3] Building LittleFS image...\n")
    log(f"  Target MCU       : {cfg.target}\n")
    log(f"  Source folder    : {cfg.folder}\n")
    log(f"  Payload size     : {payload} bytes ({human_size(payload)})\n")
    log(f"  Image size       : {cfg.image_size} bytes ({human_size(cfg.image_size)})\n")
    log(f"  Block size       : {cfg.block_size} bytes\n")
    log(f"  Page size        : {cfg.page_size} bytes\n")
    cmd = [
        cfg.mklittlefs_path,
        "-c", str(cfg.folder),
        "-b", str(cfg.block_size),
        "-p", str(cfg.page_size),
        "-s", str(cfg.image_size),
        str(cfg.fs_image),
    ]

    log("  Command          : " + " ".join(quote_arg(x) for x in cmd) + "\n")
    result = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if result.stdout.strip():
        log("  mklittlefs stdout:\n" + indent(result.stdout.rstrip()) + "\n")
    if result.stderr.strip():
        log("  mklittlefs stderr:\n" + indent(result.stderr.rstrip()) + "\n")
    if result.returncode != 0:
        raise RuntimeError(f"mklittlefs failed with exit code {result.returncode}")
    if not cfg.fs_image.exists():
        raise RuntimeError("Filesystem image was not created.")
    log(f"  Output image     : {cfg.fs_image}\n")
    log(f"  Output size      : {cfg.fs_image.stat().st_size} bytes ({human_size(cfg.fs_image.stat().st_size)})\n\n")


def merge_firmware_and_fs(cfg: BuildConfig, log) -> None:
    log("[2/3] Merging padded Firmware Binary and filesystem image...\n")
    firmware_data = cfg.firmware_binary.read_bytes()
    fs_data = cfg.fs_image.read_bytes()
    pad_len = cfg.firmware_pad_size - len(firmware_data)
    merged = firmware_data + bytes([cfg.pad_value]) * pad_len + fs_data
    cfg.merged_bin.write_bytes(merged)
    log(f"  Firmware size    : {len(firmware_data)} bytes ({human_size(len(firmware_data))})\n")
    log(f"  Pad size         : {cfg.firmware_pad_size} bytes ({human_size(cfg.firmware_pad_size)})\n")
    log(f"  Padding added    : {pad_len} bytes ({human_size(pad_len)})\n")
    log(f"  FS image size    : {len(fs_data)} bytes ({human_size(len(fs_data))})\n")
    log(f"  Pad value        : 0x{cfg.pad_value:02X}\n")
    log(f"  Output BIN       : {cfg.merged_bin}\n")
    log(f"  Merged size      : {len(merged)} bytes ({human_size(len(merged))})\n\n")


def bin_to_uf2(data: bytes, start_addr: int, payload_size: int, family_id: int) -> bytes:
    chunks = [data[i:i + payload_size] for i in range(0, len(data), payload_size)]
    total = len(chunks)
    out = bytearray()
    for index, chunk in enumerate(chunks):
        target_addr = start_addr + index * payload_size
        payload = chunk + bytes(UF2_DATA_BYTES_PER_BLOCK - len(chunk))
        header = struct.pack(
            "<IIIIIIII",
            UF2_MAGIC_START0,
            UF2_MAGIC_START1,
            UF2_FLAG_FAMILY_ID_PRESENT,
            target_addr,
            len(chunk),
            index,
            total,
            family_id,
        )
        block = header + payload + struct.pack("<I", UF2_MAGIC_END)
        if len(block) != 512:
            raise AssertionError(f"UF2 block size is {len(block)}, expected 512")
        out.extend(block)
    return bytes(out)


def convert_merged_bin_to_uf2(cfg: BuildConfig, log) -> None:
    log("[3/3] Converting merged BIN to UF2...\n")
    data = cfg.merged_bin.read_bytes()
    uf2 = bin_to_uf2(data, cfg.flash_address, cfg.uf2_payload_size, cfg.family_id)
    cfg.uf2_file.write_bytes(uf2)
    log(f"  Target MCU       : {cfg.target}\n")
    log(f"  Family ID        : 0x{cfg.family_id:08X}\n")
    log(f"  Flash address    : 0x{cfg.flash_address:08X}\n")
    log(f"  Payload size     : {cfg.uf2_payload_size} bytes\n")
    log(f"  Input BIN        : {cfg.merged_bin}\n")
    log(f"  Input size       : {len(data)} bytes ({human_size(len(data))})\n")
    log(f"  Output UF2       : {cfg.uf2_file}\n")
    log(f"  Output size      : {len(uf2)} bytes ({human_size(len(uf2))})\n")
    log(f"  UF2 blocks       : {len(uf2) // 512}\n\n")


def main() -> int:
    app = App()
    app.mainloop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
