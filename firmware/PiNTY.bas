' =========================================================================
' IntyBASIC SDK Project: PiNTY firmware
' -------------------------------------------------------------------------
'     Programmer: Yannick Erb
'     Created:    2026-13-05
'     Updated:    2026-19-05
'
' -------------------------------------------------------------------------
' History:
' 2026-13-05 - PiNTY project created.
' 2026-18-05 - Clean up all magic number, wrote abstracted interface with PI
' 2026-19-05 - Reworked file display routine
' =========================================================================

    ASM MEMATTR $8000, $9fff, "+RWN"

    OPTION EXPLICIT

	' Include useful predefined constants
	INCLUDE "constants.bas"
 	
    ' Include Sound player library
    INCLUDE "SndPlayer.bas"

	DIM I,J
    DIM #MEM
    DIM #FROM, #F_FROM, #F_TO, #F_TOTAL
    DIM Input, Debounce
    DIM Selected_Entry, Max_Entry
    DIM #Disp_Color

	CONST DEBOUNCE_DELAY  = 5					' Number of cycles to detect button press

    ' RAM addresses for exchanges with PI
    CONST ADDRESS_MAJOR     = $80FE
    CONST ADDRESS_MINOR     = $80FF
    CONST ADDRESS_TVMODE    = $8100
    CONST ADDRESS_ECS_PRES  = $8101
    CONST ADDRESS_MaxSize   = $8102 ' Max supported ROM size in kB (16 bits)
    CONST ADDRESS_AUDIO_VOL = $8104
    CONST ADDRESS_JLP_EMU   = $8105
    CONST ADDRESS_ECS_EMU   = $8106
    CONST ADDRESS_VOICE_PRES= $8107
    CONST ADDRESS_VOICE_EMU = $8108
    CONST ADDRESS_status    = $8119
    CONST ADDRESS_dev       = $8120
    CONST ADDRESS_has_sd    = $8121
    CONST ADDRESS_hw        = $8122
    CONST ADDRESS_sdpres    = $8123
    CONST ADDRESS_flist     = $817F ' 10 files * 64 characters per file (640 bytes), end address is $83FF
    CONST ADDRESS_INFO_NUM  = $8400 ' address to store the total number of info pages
    CONST ADDRESS_INFO_DISP = $8401 ' address to store the current displayed info page
    CONST ADDRESS_INFO_PAGE = $8402 ' 10 lines of 19 characters = 190 bytes, end address is $84C0
    CONST ADDRESS_cmd       = $8889
    CONST ADDRESS_err       = $888A
    CONST ADDRESS_Select    = $8899
    CONST ADDRESS_ftype     = $9000
    CONST ADDRESS_ffrom     = $9028 ' First displayed entry (16 bits)
    CONST ADDRESS_fto       = $9030 ' Last displayed entry (16 bits)
    CONST ADDRESS_ftotal    = $9032 ' Total number of entries (16 bits)
    CONST ADDRESS_path      = $9100
    CONST ADDRESS_section   = $9300
    
    ' PI current status
    CONST PI_STAT_BUZZY     = 1
    CONST PI_STAT_READY     = 0
    ' Commands that can be sent to PI
    CONST CMD_NONE          = 0
    CONST CMD_READFILELIST  = 1
    CONST CMD_RUNFILE       = 2
    CONST CMD_NEXTPAGE      = 3
    CONST CMD_PREVIOUSPAGE  = 4
    CONST CMD_UPDIRECTORY   = 5
    CONST CMD_READINFO      = 7
    CONST CMD_NEXTINFO      = 8
    CONST CMD_PREVINFO      = 9

    ' Interface actions
    CONST ACTION_NONE          = 0
    CONST ACTION_SELECT        = 1
    CONST ACTION_UP            = 2
    CONST ACTION_DOWN          = 3
    CONST ACTION_PAGEUP        = 4
    CONST ACTION_PAGEDOWN      = 5
    CONST ACTION_EXIT          = 6
    CONST ACTION_DISPHELP      = 7
    CONST ACTION_DISPINFO      = 8
    CONST ACTION_DISPSETTINGS  = 9

    ' Type of mass storage devices
    CONST DEV_FLASH         = 0
    CONST DEV_SD            = 1
    ' Type of entry
    CONST TYPE_FILE         = 0
    CONST TYPE_DIR          = 1
    ' HW
    CONST PI_HW_UNKNOWN     = 0
    CONST PI_HW_PIRTO       = 1
    CONST PI_HW_PIRTO2      = 2
    CONST PI_HW_PIRTO2SD    = 3
    CONST PI_HW_PIRTO2DUO   = 4
    CONST PI_HW_PINTY       = 5
    ' ERROR codes
    CONST ERR_NO_ERROR            = 0
    CONST ERR_COULD_NOT_OPEN_FILE = 1
    CONST ERR_FILE_TO_BIG         = 2
    ' TV MODE
    CONST isPAL          = 0
    CONST isNTSC         = 1

    CONST FNAME_LENGTH   = 64
    CONST INFO_LENGTH    = 19

    DEF FN PI_STATUS = PEEK(ADDRESS_status)
    DEF FN PI_CMD(command) = POKE(ADDRESS_cmd),command
    DEF FN PI_HAS_SD = PEEK(ADDRESS_has_sd)
    DEF FN PI_SD_PRESENT = PEEK(ADDRESS_sdpres)
    DEF FN PI_SELECTDEVICE(device) = POKE(ADDRESS_dev),device
    DEF FN PI_CURRENTDEVICE = PEEK(ADDRESS_dev)
    DEF FN PI_SELECTENTRY(entry) = POKE(ADDRESS_Select),(entry+1)
    DEF FN PI_GETENTRY = PEEK(ADDRESS_Select)
    DEF FN PI_GET_FTYPE(file) = PEEK(ADDRESS_ftype+file)
    DEF FN PI_GET_FFROM  = ((PEEK(ADDRESS_ffrom)  * 256) + PEEK(ADDRESS_ffrom+1))
    DEF FN PI_GET_FTO    = ((PEEK(ADDRESS_fto)    * 256) + PEEK(ADDRESS_fto+1))
    DEF FN PI_GET_FTOTAL = ((PEEK(ADDRESS_ftotal) * 256) + PEEK(ADDRESS_ftotal+1))
    DEF FN PI_GET_HW     = PEEK(ADDRESS_hw)
    DEF FN PI_GET_ERROR  = PEEK(ADDRESS_err)
    DEF FN PI_GET_INFO_NUM = PEEK(ADDRESS_INFO_NUM)
    DEF FN PI_GET_INFO_DISP = PEEK(ADDRESS_INFO_DISP)
    DEF FN PI_SET_TVMODE(mode) = POKE(ADDRESS_TVMODE),mode
    DEF FN PI_SET_ECS_PRES(presence) = POKE(ADDRESS_ECS_PRES),presence
    DEF FN PI_SET_VOICE_PRES(presence) = POKE(ADDRESS_VOICE_PRES),presence
    DEF FN PI_SET_AUDIO_VOL(v) = POKE(ADDRESS_AUDIO_VOL),v
    DEF FN PI_GET_AUDIO_VOL = PEEK(ADDRESS_AUDIO_VOL)
    DEF FN PI_GET_MAXSIZE = ((PEEK(ADDRESS_MaxSize) * 256) + PEEK(ADDRESS_MaxSize+1))
    DEF FN PI_GET_JLP_EMU = PEEK(ADDRESS_JLP_EMU)
    DEF FN PI_GET_ECS_EMU = PEEK(ADDRESS_ECS_EMU)
    DEF FN PI_GET_VOICE_EMU = PEEK(ADDRESS_VOICE_EMU)
    DEF FN PI_GET_MAJOR = PEEK(ADDRESS_MAJOR)
    DEF FN PI_GET_MINOR = PEEK(ADDRESS_MINOR)


    ' Display splash screen
	MODE 0,0,2,0,2
	WAIT
	DEFINE 0,16,screen_bitmaps_0
	WAIT
	DEFINE 16,16,screen_bitmaps_1
	WAIT
	DEFINE 32,16,screen_bitmaps_2
	WAIT
	SCREEN screen_cards,0,5,11,7,11

    ' Display firmware version number
    PRINT AT SCREENPOS(16,11) COLOR CS_YELLOW,"v",<1>PI_GET_MAJOR,".",<1>PI_GET_MINOR
    
    ' Display text
    DEFINE 48,16,text_bitmaps_0
    FOR I=0 TO 15
        #BACKTAB(182 +  I) = $0987 +  I * 8
    NEXT I

    PlaySnd(WelcomeSound)
    FOR I = 1 TO 120:WAIT:NEXT I
    ' Next 10 animation frames for text
    FOR I=1 TO 10 
        DEFINE 48,16,VARPTR text_bitmaps_0(64 * I)
        FOR J=0 TO 7:WAIT:NEXT J
    NEXT I

    ' Wait for card to be ready and minimum delay
    I = 30
    WHILE (PI_STATUS<>PI_STAT_READY) OR (I>0)
        IF I>0 THEN I = I - 1
        WAIT
    WEND

    ' Send INTY condfiguration to PI
    PI_SET_TVMODE(NTSC)
    PI_SET_ECS_PRES(ECS.AVAILABLE)
    PI_SET_VOICE_PRES(VOICE.AVAILABLE)
    
    CLS
    WAIT

    ' Get current directory informations
    #f_from  = PI_GET_FFROM
    #f_to    = PI_GET_FTO

    IF #f_from < #f_to THEN
        ' RUN GAME
        PI_SELECTENTRY(0)
        PI_CMD(CMD_RUNFILE)
    ELSE
        ' No file on card => display error message
        PRINT AT SCREENPOS(2,6) COLOR CS_RED, "NO FILE ON CARD"
    END IF
    ' Infinite loop till getting reseted by card
    ASM InfiniteLoop:
    ASM    B InfiniteLoop

'
' DATA
'
    INCLUDE "Pinty_logo.bas"
    INCLUDE "text.bas"
    INCLUDE "Sounds.bas"
