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
    CONST ADDRESS_TVMODE    = $8100
    CONST ADDRESS_ECS_PRES  = $8101
    CONST ADDRESS_status    = $8119
    CONST ADDRESS_cmd       = $8889
    CONST ADDRESS_has_sd    = $8121
    CONST ADDRESS_dev       = $8120
    CONST ADDRESS_Select    = $8899
    CONST ADDRESS_flist     = $817F
    CONST ADDRESS_ftype     = $9000
    CONST ADDRESS_ffrom     = $9028 ' First displayed entry
    CONST ADDRESS_fto       = $9030 ' Last displayed entry
    CONST ADDRESS_ftotal    = $9032 ' Total number of entries
    CONST ADDRESS_hw        = $8122
    
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
    CONST CMD_SWITCHDEVICE  = 6
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
    ' TV MODE
    CONST isPAL          = 0
    CONST isNTSC         = 1
    ' ECS Presence
    CONST ECS_Absent     = 0 
    CONST ECS_Present    = 1

    DEF FN PI_STATUS = PEEK(ADDRESS_status)
    DEF FN PI_CMD(command) = POKE(ADDRESS_cmd),command
    DEF FN PI_HAS_SD = PEEK(ADDRESS_has_sd)
    DEF FN PI_SELECTDEVICE(device) = POKE(ADDRESS_dev),device
    DEF FN PI_CURRENTDEVICE = PEEK(ADDRESS_dev)
    DEF FN PI_SELECTENTRY(entry) = POKE(ADDRESS_Select),(entry+1)
    DEF FN PI_GET_FTYPE(file) = PEEK(ADDRESS_ftype+file)
    DEF FN PI_GET_FFROM  = ((PEEK(ADDRESS_ffrom)  * 256) + PEEK(ADDRESS_ffrom+1))
    DEF FN PI_GET_FTO    = ((PEEK(ADDRESS_fto)    * 256) + PEEK(ADDRESS_fto+1))
    DEF FN PI_GET_FTOTAL = ((PEEK(ADDRESS_ftotal) * 256) + PEEK(ADDRESS_ftotal+1))
    DEF FN PI_GET_HW     = PEEK(ADDRESS_hw)
    DEF FN PI_SET_TVMODE(mode) = POKE(ADDRESS_TVMODE),mode
    DEF FN PI_SET_ECS_PRES(presence) = POKE(ADDRESS_ECS_PRES),presence
    
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
    IF (NTSC <>0) THEN PI_SET_TVMODE(isNTSC) ELSE PI_SET_TVMODE(isPAL)
    IF (ECS.AVAILABLE <> 0) THEN PI_SET_ECS_PRES(ECS_Present) ELSE PI_SET_ECS_PRES(ECS_Absent)
    
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
