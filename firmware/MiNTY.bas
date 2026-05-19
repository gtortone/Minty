' =========================================================================
' IntyBASIC SDK Project: MiNTY firmware
' -------------------------------------------------------------------------
'     Programmer: Yannick Erb
'     Created:    2026-13-05
'     Updated:    2026-19-05
'
' -------------------------------------------------------------------------
' History:
' 2026-13-05 - MiNTY project created.
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
    CONST ADDRESS_status    = $8119
    CONST ADDRESS_cmd       = $8889
    CONST ADDRESS_has_sd    = $8121
    CONST ADDRESS_dev       = $8120
    CONST ADDRESS_Select    = $8899
    CONST ADDRESS_flist     = $817F
    CONST ADDRESS_ftype     = $9000
    CONST ADDRESS_ffrom     = $9028 ' First displyed entry
    CONST ADDRESS_fto       = $9030 ' Last displayed entry
    CONST ADDRESS_ftotal    = $9032 ' Total number of entries
    CONST ADDRESS_hw        = $8122
    
    ' PI current status
    CONST PI_STAT_START     = 123
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

    ' Display detected HW
    I = PI_GET_HW
    IF I=0 THEN PRINT AT SCREENPOS(0, 11) COLOR CS_WHITE,"HW : Unknown"
    IF I=1 THEN PRINT AT SCREENPOS(0, 11) COLOR CS_WHITE,"HW : PiRTO"
    IF I=2 THEN PRINT AT SCREENPOS(0, 11) COLOR CS_WHITE,"HW : PiRTO2"
    IF I=3 THEN PRINT AT SCREENPOS(0, 11) COLOR CS_WHITE,"HW : PiRTO2+SD"
    IF I=4 THEN PRINT AT SCREENPOS(0, 11) COLOR CS_WHITE,"HW : PiRTO2-DUO"
    IF I=5 THEN PRINT AT SCREENPOS(0, 11) COLOR CS_WHITE,"HW : PiNTY"

    ' Display text
    DEFINE 48,16,text_bitmaps_0
    FOR I=0 TO 15
        #BACKTAB(182 +  I) = $0987 +  I * 8
    NEXT I

    PlaySnd(WelcomeSound)
    FOR I = 1 TO 60:WAIT:NEXT I
    ' Next 10 animation frames for text
    FOR I=1 TO 10 
        DEFINE 48,16,VARPTR text_bitmaps_0(64 * I)
        FOR J=0 TO 7:WAIT:NEXT J
    NEXT I

	' Wait for card to be ready and minimum delay
	I = 30
    WHILE (PI_STATUS<>PI_STAT_START) OR (I>0)
        IF I>0 THEN I = I - 1
        WAIT
    WEND

    ' Load waiting animation frames into GRAM
    CLS
    DEFINE 0,10,In_Progress
    WAIT
    DEFINE 10,10,VARPTR In_Progress(40)
    WAIT
    ' load icons into GRAM
    DEFINE 20,2, Icons
    WAIT

    ' Start up with selecting the first entry
    Selected_Entry=0

START:
    CLS
    PRINT AT SCREENPOS(8, 0) COLOR CS_WHITE, "MiNTY"

    ' Display active device
    IF PI_CURRENTDEVICE = DEV_FLASH THEN
       PRINT AT SCREENPOS(0, 11) COLOR CS_WHITE, "FL:"
    ELSE
       PRINT AT SCREENPOS(0, 11) COLOR CS_WHITE, "SD:"
    END IF

    ' Get current directory informations
    #f_from  = PI_GET_FFROM
    #f_to    = PI_GET_FTO
    #f_total = PI_GET_FTOTAL

    IF #f_from = #f_to THEN ' empty list
        #from=0
    ELSE
        #from=#f_from+1
    END IF

    PRINT AT SCREENPOS(3, 11) COLOR CS_WHITE, <3>#from, "-", <3>#f_to, "/", <3>#f_total, " 1:HLP"

    ' Display file list
    GOSUB DISPLAY_FILELIST

MENU_LOOP:
    WAIT
    IF Debounce>0 THEN Debounce = Debounce-1:GOTO MENU_LOOP

    Input = CONT

    ' SELECT
    IF (Input=$28) THEN
        Debounce = DEBOUNCE_DELAY
        GOSUB SELECT_ENTRY
        GOTO START
    END IF

    ' UPDIR
    IF (Input=$88) THEN
        Debounce = DEBOUNCE_DELAY
        GOSUB UP_DIRECTORY
        GOTO START
    END IF    
    
    ' HELP
    IF (Input=$81) THEN
        Debounce = DEBOUNCE_DELAY
        GOSUB HELP_SCREEN
        GOTO START
    END IF

    ' DOWN
    IF (Input=$48 OR Input=$01 OR Input=$11 OR Input=$19 OR Input=$09 OR Input=$03 OR Input=$13 OR Input=$12) THEN      'KEYPAD_0 or S/SE/SW
        Debounce = DEBOUNCE_DELAY
        IF ((Selected_Entry=Max_Entry) AND (#f_to < #f_total)) THEN
            ' first entry of next page will be selected
            GOSUB NEXT_PAGE
            GOTO START
        END IF
        IF Selected_Entry < Max_Entry THEN
            Selected_Entry=Selected_Entry+1
            PlaySnd(InputSound)
            GOSUB DISPLAY_FILELIST
        END IF
    END IF
    
    ' UP
    IF (Input=$44 or Input=$04 or Input=$0C or Input=$1C or Input=$18 or Input=$14 or Input=$16 or Input=$06) THEN    'KEYPAD_8 or N/NNW/NNE
        Debounce = DEBOUNCE_DELAY
        IF (Selected_Entry=0 and #f_from>0) THEN 
            ' last entry of previous page will be selected
            Selected_Entry = 9
            GOSUB PREVIOUS_PAGE
            GOTO START
        END IF
        IF Selected_Entry>0 THEN
            Selected_Entry=Selected_Entry-1 
            PlaySnd(InputSound)
            GOSUB DISPLAY_FILELIST
        END IF
    END IF

    ' PAGE_DOWN
    IF ((Input=$60 or Input=$C0 or Input=$24) AND (#f_to < #f_total)) THEN     'b-left or b-right or KEYPAD_9
        Debounce = DEBOUNCE_DELAY
        GOSUB NEXT_PAGE
        GOTO START
    END IF
 
    ' PAGE_UP
    IF ((Input=$A0 OR Input=$84) AND (#f_from > 0)) THEN    'b-top or KEYPAD_7
        Debounce = DEBOUNCE_DELAY
        Selected_Entry = 0
        GOSUB PREVIOUS_PAGE
        GOTO START
    END IF

    ' CHANGE STORAGE DEVICE
    IF (Input=$21 and PI_HAS_SD=1) THEN     ' KEYPAD_3
        Debounce = DEBOUNCE_DELAY
        Selected_Entry = 0
        GOSUB CHANGE_DEVICE
        GOTO START
    END IF

    GOTO MENU_LOOP

'
' PROCEDURES
'
CHANGE_DEVICE: PROCEDURE
    PI_SELECTDEVICE(1-PI_CURRENTDEVICE)    'switch device
    PI_CMD(CMD_SWITCHDEVICE)
    PlaySnd(InputSound)
    GOSUB WAIT_CARD_ANSWER
    END

NEXT_PAGE: PROCEDURE
    PI_CMD(CMD_NEXTPAGE)
    PlaySnd(InputSound)
    GOSUB WAIT_CARD_ANSWER
    Selected_Entry = 0
    END

PREVIOUS_PAGE: PROCEDURE
    PI_CMD(CMD_PREVIOUSPAGE)
    PlaySnd(InputSound)
    GOSUB WAIT_CARD_ANSWER
    END

HELP_SCREEN: PROCEDURE
    PlaySnd(InputSound)
    CLS
    PRINT AT SCREENPOS(8,0) COLOR CS_GREEN, "HELP"
    PRINT AT SCREENPOS(0,2) COLOR CS_WHITE, "8 or up:   go up"
    PRINT AT SCREENPOS(0,3) COLOR CS_WHITE, "0 or down: go dn"
    PRINT AT SCREENPOS(0,4) COLOR CS_WHITE, "7 or b-up: page up"
    PRINT AT SCREENPOS(0,5) COLOR CS_WHITE, "9 or b-dn: page dn"
    IF PI_HAS_SD=1 THEN
        PRINT AT SCREENPOS(0,7) COLOR CS_WHITE, "3:         sw FL/SD"
    END IF
    PRINT AT SCREENPOS(0,8) COLOR CS_WHITE, "CLEAR:     dir up"
    PRINT AT SCREENPOS(0,9) COLOR CS_WHITE, "ENTER:     select"
    PRINT AT SCREENPOS(0,11) COLOR CS_YELLOW, "  <CLEAR> to exit"
    WHILE (CONT <> $88)  'CLEAR
        WAIT
    WEND
    END

SELECT_ENTRY: PROCEDURE
    IF #f_from < #f_to THEN
        PI_SELECTENTRY(Selected_Entry)
        PI_CMD(CMD_RUNFILE)
        IF PI_GET_FTYPE(Selected_Entry) = TYPE_DIR THEN 
            GOSUB WAIT_CARD_ANSWER
            Selected_Entry = 0
        ELSE
            ' RUN GAME
            GOSUB WAIT_CARD_ANSWER
            ' Infinite loop till getting reseted by card
            ASM InfiniteLoop:
            ASM    B InfiniteLoop
        END IF
    END IF
    END

UP_DIRECTORY: PROCEDURE
    PI_CMD(CMD_UPDIRECTORY)
    PlaySnd(InputSound)
    CLS
    PRINT AT  43 COLOR CS_TAN,"Up directory"
    GOSUB WAIT_CARD_ANSWER
    Selected_Entry = 0
    END

WAIT_CARD_ANSWER: PROCEDURE
    I = 0
    WHILE PI_STATUS = PI_STAT_BUZZY
		SPRITE 0, 84 + VISIBLE, 56 + ZOOMY2, SPR00 + I*8 + SPR_RED
        I = (I+1)
        IF I=20 THEN I=0
        WAIT:WAIT
    WEND
	ResetSprite(0)
    END

DISPLAY_FILELIST: PROCEDURE
    Max_Entry = 9

    FOR J=0 TO 9
        #mem = PEEK((ADDRESS_flist)+40*J)
        IF #mem>0 THEN
            IF PI_GET_FTYPE(J)=TYPE_DIR THEN
                IF J = Selected_Entry THEN #Disp_Color = CS_GREEN ELSE #Disp_Color = CS_BLUE
                PRINT AT screenpos(0,J+1),  BG20 + CS_YELLOW
            ELSE
                IF J = Selected_Entry THEN #Disp_Color = CS_GREEN ELSE #Disp_Color = CS_TAN
                PRINT AT screenpos(0,J+1),  BG21 + CS_CYAN
            END IF
            FOR I=0 TO 18
                #mem = PEEK((ADDRESS_flist+I*2)+40*J)
                IF #mem<32 THEN #mem=0 ELSE #mem=#mem-32
                IF #mem=63 THEN #mem=207 ' correct replacement for underscore
                PRINT #mem*8+#Disp_Color
            NEXT I
        ELSE
            Max_Entry = Max_Entry - 1
        END IF
    NEXT J
  END

'
' DATA
'
    INCLUDE "Minty_logo.bas"
    INCLUDE "text.bas"
    INCLUDE "Sounds.bas"

Icons:
    DATA $FEE0,$82E2,$8282,$00FE
    DATA $4438,$CED6,$54D6,$0038

In_Progress:
    DATA $002C,$0000,$0000,$0000
    DATA $0214,$0000,$0000,$0000
    DATA $0208,$0001,$0000,$0000
    DATA $0004,$0101,$0000,$0000
    DATA $0200,$0100,$0001,$0000
    DATA $0000,$0001,$0101,$0000
    DATA $0000,$0100,$0100,$0002
    DATA $0000,$0000,$0001,$0402
    DATA $0000,$0000,$0100,$0C00
    DATA $0000,$0000,$0000,$1802
    DATA $0000,$0000,$0000,$3400
    DATA $0000,$0000,$0000,$2840
    DATA $0000,$0000,$8000,$1040
    DATA $0000,$0000,$8080,$2000
    DATA $0000,$8000,$0080,$0040
    DATA $0000,$8080,$8000,$0000
    DATA $4000,$0080,$0080,$0000
    DATA $4020,$8000,$0000,$0000
    DATA $0030,$0080,$0000,$0000
    DATA $4018,$0000,$0000,$0000