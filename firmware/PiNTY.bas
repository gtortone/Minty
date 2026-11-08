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
    DIM Input, Debounce, Volume, ACTION, WaveFrame
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

    Input = 0

    PlaySnd(WelcomeSound)
    FOR I = 1 TO 60
        IF CONT.KEY = 11 THEN Input = 1
        WAIT
    NEXT I
    ' Next 10 animation frames for text
    FOR I=1 TO 10 
        DEFINE 48,16,VARPTR text_bitmaps_0(64 * I)
        FOR J=0 TO 7
            IF CONT.KEY = 11 THEN Input = 1
            WAIT
        NEXT J
    NEXT I

    ' Wait for card to be ready and minimum delay
    I = 20
    WHILE (PI_STATUS<>PI_STAT_READY) OR (I>0)
        IF I>0 THEN I = I - 1
        IF CONT.KEY = 11 THEN Input = 1
        WAIT
    WEND

    ' Send INTY condfiguration to PI
    PI_SET_TVMODE(NTSC)
    PI_SET_ECS_PRES(ECS.AVAILABLE)
    IF VOICE.AVAILABLE = $8001 THEN PI_SET_VOICE_PRES(1) ELSE PI_SET_VOICE_PRES(0)
    Volume = PI_GET_AUDIO_VOL
    IF Volume = 0 THEN Volume = 180 ' If no volume set start at 180 
    
    IF Input = 1 THEN GOSUB SETTINGS_SCREEN

    PI_SET_AUDIO_VOL(Volume)
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

SETTINGS_SCREEN: PROCEDURE
    PlaySnd(InputSound)
    GOSUB DISPLAY_HEADER
    PRINT AT SCREENPOS(9, 0) COLOR CS_YELLOW, "Information"

    PRINT AT SCREENPOS( 1,2) COLOR CS_TAN, "Firmware"
    PRINT AT SCREENPOS(15,2) COLOR CS_DARKGREEN,"v",<1>PI_GET_MAJOR,".",<1>PI_GET_MINOR
  
    PRINT AT SCREENPOS( 1,3)  COLOR CS_TAN, "Tv Standard"
    IF NTSC THEN 
        PRINT AT SCREENPOS(15, 3) COLOR CS_DARKGREEN,"NTSC"
    ELSE
        PRINT AT SCREENPOS(16, 3) COLOR CS_DARKGREEN,"PAL"
    END IF

    PRINT AT SCREENPOS( 1, 4) COLOR CS_TAN, "JLP emulation"
    IF PI_GET_JLP_EMU THEN
        PRINT AT SCREENPOS(16, 4) COLOR CS_DARKGREEN,"YES"
    ELSE
        PRINT AT SCREENPOS(17, 4) COLOR CS_DARKGREEN,"NO"
    END IF

    PRINT AT SCREENPOS( 1, 5) COLOR CS_TAN, "Max ROM size"
    PRINT AT SCREENPOS(14, 5) COLOR CS_DARKGREEN, <3>PI_GET_MAXSIZE,"kB"

    PRINT AT SCREENPOS( 1, 7) COLOR CS_TAN, "         PRES  EMU"
    PRINT AT SCREENPOS( 1, 8) COLOR CS_TAN, "ECS"
    PRINT AT SCREENPOS( 1, 9) COLOR CS_TAN, "iVoice"
    IF ECS.AVAILABLE THEN 
        PRINT AT SCREENPOS(11, 8) COLOR CS_DARKGREEN,"YES"
    ELSE
        PRINT AT SCREENPOS(12, 8) COLOR CS_DARKGREEN,"NO"
    END IF
    IF PI_GET_ECS_EMU THEN
        PRINT AT SCREENPOS(16, 8) COLOR CS_DARKGREEN,"YES"   
    ELSE
        PRINT AT SCREENPOS(17, 8) COLOR CS_DARKGREEN,"NO"   
    END IF
    IF (VOICE.AVAILABLE = $8001) THEN 
        PRINT AT SCREENPOS(11, 9) COLOR CS_DARKGREEN,"YES"
    ELSE
        PRINT AT SCREENPOS(12, 9) COLOR CS_DARKGREEN,"NO"
    END IF
    IF PI_GET_VOICE_EMU THEN
        PRINT AT SCREENPOS(16, 9) COLOR CS_DARKGREEN,"YES"   
    ELSE
        PRINT AT SCREENPOS(17, 9) COLOR CS_DARKGREEN,"NO"   
    END IF    
    Volume = PI_GET_AUDIO_VOL
    PRINT AT SCREENPOS( 1, 10)  COLOR CS_TAN, "Volume  UP/DN"
    PRINT AT SCREENPOS(16, 10) COLOR CS_GREEN,<3>Volume


    PRINT AT SCREENPOS( 2,11) COLOR CS_YELLOW, "<CLEAR>  to exit"
    WHILE (CONT <> $88)  'CLEAR
        IF PI_GET_ECS_EMU THEN
            IF Debounce > 0 THEN
                Debounce = Debounce - 1
            ELSE
                GOSUB GetAction

                IF ((ACTION = ACTION_DOWN) AND (Volume > 0)) THEN Volume = Volume - 1
                IF (ACTION = ACTION_PAGEUP) THEN IF (Volume > 10) THEN Volume = Volume-10 ELSE Volume = 0
                IF ((ACTION = ACTION_UP) AND (Volume < 255)) THEN Volume = Volume + 1
                IF (ACTION = ACTION_PAGEDOWN) THEN IF (Volume < 245) THEN Volume = Volume+10 ELSE Volume = 255
                PRINT AT SCREENPOS(16, 10) COLOR CS_GREEN,<3>Volume
            END IF
        END IF
        IF FRAME%8 = 0 THEN
            WaveFrame = (WaveFrame+1)%4
            DEFINE 6,1,VARPTR Title_wave(WaveFrame * 4)
        END IF
        WAIT
    WEND
    PI_SET_AUDIO_VOL(Volume)
    Debounce = DEBOUNCE_DELAY
    END

DISPLAY_HEADER: PROCEDURE
     CLS
    MODE 0,6,0,7,0
    ' load title wave
    DEFINE 6,1,Title_wave
    WAIT
    ' load title text graphics
    DEFINE 7,7,Title_text
    WAIT
    ResetSprite(1)
    ResetSprite(2)
    ResetSprite(3)
    SCREEN Title_cards,0,0,8,1,8
    SPRITE 4,  8 + VISIBLE + ZOOMX2, 11 , SPR06 + BEHIND + SPR_ORANGE
    SPRITE 5, 24 + VISIBLE + ZOOMX2, 11 , SPR06 + BEHIND + SPR_ORANGE
    SPRITE 6, 40 + VISIBLE + ZOOMX2, 11 , SPR06 + BEHIND + SPR_ORANGE
    SPRITE 7, 56 + VISIBLE + ZOOMX2, 11 , SPR06 + BEHIND + SPR_ORANGE
    WaveFrame = 0
    END

GetAction: PROCEDURE
    ' Get user input and return it in CONT variable
    Input = CONT
    Action = ACTION_NONE
    IF Input <> 0 THEN
        FOR I = 0 TO 30
            IF Input = ContCode(I) THEN Action = MainAction(I):Debounce = DEBOUNCE_DELAY:RETURN
        NEXT I
    END IF
    END

'
' DATA
'
Title_text:
	DATA $183C,$2400,$3C3C,$FF3C
	DATA $9193,$90F0,$9392,$FF93
	DATA $9090,$1C9C,$1C1C,$FF9C
	DATA $2327,$F8F0,$FCFC,$FFFC
	DATA $1E9E,$7E3E,$FEFE,$FFFE
	DATA $0909,$3C79,$7E3C,$FF7E
	DATA $F3F3,$A7F3,$0FA7,$FF4F

Title_Wave:
    DATA $F3C0,$FFFF,$FFFF,$FFFF
    DATA $FC30,$FFFF,$FFFF,$FFFF
    DATA $3F0C,$FFFF,$FFFF,$FFFF
    DATA $CF03,$FFFF,$FFFF,$FFFF

Title_cards:
	DATA $0838,$0840,$0848,$0850,$0858,$0860,$0868,$22F8

ContCode:
    DATA 129,65,33,130,66,34,132,68,36,136,72,40,160,96,192,4,20,22,6,2,18,19,3,1,17,25,9,8,24,28,12

MainAction:
    DATA ACTION_DISPHELP        ' 1
    DATA ACTION_DISPINFO        ' 2
    DATA ACTION_DISPSETTINGS    ' 3
    DATA ACTION_NONE            ' 4
    DATA ACTION_NONE            ' 5
    DATA ACTION_NONE            ' 6
    DATA ACTION_PAGEDOWN        ' 7
    DATA ACTION_UP              ' 8
    DATA ACTION_PAGEUP          ' 9
    DATA ACTION_EXIT            ' CLR
    DATA ACTION_DOWN            ' 0
    DATA ACTION_SELECT          ' ENT
    DATA ACTION_PAGEUP          ' BUT_UP
    DATA ACTION_PAGEDOWN        ' BUT_LEFT
    DATA ACTION_PAGEDOWN        ' BUT_RIGHT
    DATA ACTION_UP              ' N
    DATA ACTION_UP              ' NNE
    DATA ACTION_NONE            ' NE
    DATA ACTION_NONE            ' NEE
    DATA ACTION_PAGEDOWN        ' E
    DATA ACTION_NONE            ' SEE
    DATA ACTION_NONE            ' SE
    DATA ACTION_DOWN            ' SSE
    DATA ACTION_DOWN            ' S
    DATA ACTION_DOWN            ' SSW
    DATA ACTION_NONE            ' SW
    DATA ACTION_NONE            ' SWW
    DATA ACTION_PAGEUP          ' W
    DATA ACTION_NONE            ' NWW
    DATA ACTION_NONE            ' NW
    DATA ACTION_UP              ' NNW

    INCLUDE "Pinty_logo.bas"
    INCLUDE "text.bas"
    INCLUDE "Sounds.bas"
