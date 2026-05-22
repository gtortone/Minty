import sys
import os


def help():
    print("usage: bin2header <filename> <array-name>")
    print("write to file: bin2header <filename> <array-name> > header.h")


def main():
    if len(sys.argv) < 3:
        help()
        sys.exit(-1)

    filename = sys.argv[1]
    array_name = sys.argv[2]

    if not os.path.isfile(filename):
        print("cant open...")
        help()
        sys.exit(-1)

    size = os.path.getsize(filename)

    print("/*")
    print(" * bin2header.py ")
    print(f" * file '{filename}', filesize {size} bytes")
    print(" *")
    print(" */\n")

    print(f"uint8_t {array_name}[{size}] = {{")

    count = 0

    with open(filename, "rb") as f:
        while count < size:
            print("\t", end="")
            for _ in range(8):
                byte = f.read(1)
                if not byte:
                    break

                value = byte[0]
                print(f"0x{value:02x}", end="")

                count += 1
                if count == size:
                    break
                else:
                    print(", ", end="")

            print()

    print("};")


if __name__ == "__main__":
    main()

