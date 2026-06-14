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

	DIM I,J,K,WaveFrame
    DIM #F_FROM, #F_TO, #F_TOTAL
    DIM Input, Debounce
    DIM Selected_Entry, Max_Entry
    DIM #Disp_Color, #tmp
    DIM SelEnt_Start,SelEnt_Length

	CONST DEBOUNCE_DELAY  = 5					' Number of cycles to detect button press

    ' RAM addresses for exchanges with PI
    CONST ADDRESS_TVMODE    = $8100
    CONST ADDRESS_ECS_PRES  = $8101
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
    CONST CMD_SWITCHDEVICE  = 6
    CONST CMD_READINFO      = 7
    CONST CMD_NEXTINFO      = 8
    CONST CMD_PREVINFO      = 9
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
    ' ECS Presence
    CONST ECS_Absent     = 0 
    CONST ECS_Present    = 1

    CONST FNAME_LENGTH   = 64
    CONST INFO_LENGTH    = 19

    DEF FN PI_STATUS = PEEK(ADDRESS_status)
    DEF FN PI_CMD(command) = POKE(ADDRESS_cmd),command
    DEF FN PI_HAS_SD = PEEK(ADDRESS_has_sd)
    DEF FN PI_SD_PRESENT = PEEK(ADDRESS_sdpres)
    DEF FN PI_SELECTDEVICE(device) = POKE(ADDRESS_dev),device
    DEF FN PI_CURRENTDEVICE = PEEK(ADDRESS_dev)
    DEF FN PI_SELECTENTRY(entry) = POKE(ADDRESS_Select),(entry+1)
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
    IF I=1 THEN 
        PRINT AT SCREENPOS(0, 11) COLOR CS_WHITE,"HW : PiRTO"
    ELSEIF I=2 THEN 
        PRINT AT SCREENPOS(0, 11) COLOR CS_WHITE,"HW : PiRTO2"
    ELSEIF I=3 THEN 
        PRINT AT SCREENPOS(0, 11) COLOR CS_WHITE,"HW : PiRTO2+SD"
    ELSEIF I=4 THEN
        PRINT AT SCREENPOS(0, 11) COLOR CS_WHITE,"HW : PiRTO2-DUO"
    ELSEIF I=5 THEN 
        PRINT AT SCREENPOS(0, 11) COLOR CS_WHITE,"HW : PiNTY"
    ELSE 
        PRINT AT SCREENPOS(0, 11) COLOR CS_WHITE,"HW : Unknown"
    END IF
    
    ' Display text
    DEFINE 48,16,text_bitmaps_0
    FOR I=0 TO 15
        #BACKTAB(182 +  I) = $0987 +  I * 8
    NEXT I

    PlaySnd(WelcomeSound)
    FOR I = 1 TO 90:WAIT:NEXT I
    ' Next 10 animation frames for text
    FOR I=1 TO 10 
        DEFINE 48,16,VARPTR text_bitmaps_0(64 * I)
        FOR J=0 TO 5:WAIT:NEXT J
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
    MODE 0,6,0,7,0
    ' load icons into GRAM
    DEFINE 0,2,Icons
    WAIT
    ' load side slider graphics
    DEFINE 2,4,Slider
    WAIT
    ' load title wave
    DEFINE 6,1,Title_wave
    WAIT
    ' load title text graphics
    DEFINE 7,7,Title_text
    WAIT
    ' load help graphics
    DEFINE 14,3,Help_text
    WAIT

    ' GRAM 17 to 22 are free

    ' load font into grams (23 to 63 are occupied)
    GOSUB Init_font

    ' Start up with selecting the first entry
    Selected_Entry=0

'    GOSUB test_init

START:
    CLS
    SCREEN Title_cards,0,0,8,1,8
    SPRITE 4,  8 + VISIBLE + ZOOMX2, 11 , SPR06 + BEHIND + SPR_ORANGE
    SPRITE 5, 24 + VISIBLE + ZOOMX2, 11 , SPR06 + BEHIND + SPR_ORANGE
    SPRITE 6, 40 + VISIBLE + ZOOMX2, 11 , SPR06 + BEHIND + SPR_ORANGE
    SPRITE 7, 56 + VISIBLE + ZOOMX2, 11 , SPR06 + BEHIND + SPR_ORANGE
    WaveFrame = 0

    SCREEN Help_Cards,0,17,3,1,3

    ' Display current path
    #tmp = PEEK(ADDRESS_path + 1) AND $7F
    ' First letter need to change Color Stack
    #BACKTAB(20) = ASCII_table(#tmp) + CS_GREEN + $2000
    FOR I = 1 TO 19
        #tmp = PEEK(ADDRESS_path + I + 1) AND $7F
        #BACKTAB(20+I) = ASCII_table(#tmp) + CS_GREEN
    NEXT I

    ' Get current directory informations and display slider
    #f_from  = PI_GET_FFROM
    #f_to    = PI_GET_FTO
    #f_total = PI_GET_FTOTAL  

    ' Diplay Slider
    GOSUB DISPLAY_SLIDER

    ' Display file list
    GOSUB DISPLAY_FILELIST

MENU_LOOP:
    WAIT
    ' Update animation frame
    IF FRAME%8 = 0 THEN
        WaveFrame = (WaveFrame+1)%4
        DEFINE 6,1,VARPTR Title_wave(WaveFrame * 4)
    END IF

    ' Update selected file display is length > display size
    IF FRAME%16 = 0 THEN
        GOSUB DISPLAY_SELECTED_ENTRY
    END IF

    IF Debounce>0 THEN Debounce = Debounce-1:GOTO MENU_LOOP

    Input = CONT

    ' SELECT
    IF (Input=$28) THEN ' ENTER
        Debounce = DEBOUNCE_DELAY
        GOSUB SELECT_ENTRY
        GOTO START
    END IF

    ' UPDIR
    IF (Input=$88) THEN ' CLEAR
        Debounce = DEBOUNCE_DELAY
        GOSUB UP_DIRECTORY
        GOTO START
    END IF    
    
    ' HELP
    IF (Input=$81) THEN ' KEYPAD_1
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

    ' Not supported anymore
    ' CHANGE STORAGE DEVICE
    'IF (Input=$21 and PI_HAS_SD=1) THEN     ' KEYPAD_3
    '    Debounce = DEBOUNCE_DELAY
    '    Selected_Entry = 0
    '    GOSUB CHANGE_DEVICE
    '    GOTO START
    'END IF

    ' DISPLAY FILE INFORMATION
    IF (Input=$41) THEN     ' KEYPAD_2
        Debounce = DEBOUNCE_DELAY
        GOSUB INFO_SCREEN
        GOTO START
    END IF
   
    GOTO MENU_LOOP

'
' PROCEDURES
'
' Not supported anymore
'CHANGE_DEVICE: PROCEDURE
'    PI_SELECTDEVICE(1-PI_CURRENTDEVICE)    'switch device
'    PI_CMD(CMD_SWITCHDEVICE)
'    PlaySnd(InputSound)
'    GOSUB WAIT_CARD_ANSWER
'    END

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
    ResetSprite(1)
    ResetSprite(2)
    ResetSprite(3)
    SCREEN Title_cards,0,0,8,1,8
    SPRITE 4,  8 + VISIBLE + ZOOMX2, 11 , SPR06 + BEHIND + SPR_ORANGE
    SPRITE 5, 24 + VISIBLE + ZOOMX2, 11 , SPR06 + BEHIND + SPR_ORANGE
    SPRITE 6, 40 + VISIBLE + ZOOMX2, 11 , SPR06 + BEHIND + SPR_ORANGE
    SPRITE 7, 56 + VISIBLE + ZOOMX2, 11 , SPR06 + BEHIND + SPR_ORANGE
    PRINT AT SCREENPOS(11,0) COLOR CS_YELLOW, "Main help"
    PRINT AT SCREENPOS(1,2) COLOR CS_TAN, "8 or UP      Go Up"
    PRINT AT SCREENPOS(1,3) COLOR CS_TAN, "0 or DN      Go Dn"
    PRINT AT SCREENPOS(1,4) COLOR CS_TAN, "7 or B-UP  Page Up"
    PRINT AT SCREENPOS(1,5) COLOR CS_TAN, "9 or B-DN  Page Dn"
    ' Not supported anymore
    'IF PI_HAS_SD=1 THEN
    '    PRINT AT SCREENPOS(0,6) COLOR CS_TAN, "3:         sw FL/SD"
    'END IF
    PRINT AT SCREENPOS(1,7) COLOR CS_TAN, "2        File Info"
    PRINT AT SCREENPOS(1,8) COLOR CS_TAN, "CLEAR       Dir Up"
    PRINT AT SCREENPOS(1,9) COLOR CS_TAN, "ENTER       Select"
    PRINT AT SCREENPOS(3,11) COLOR CS_YELLOW, "<CLR>  to exit"
    WHILE (CONT <> $88)  'CLEAR
        IF FRAME%8 = 0 THEN
            WaveFrame = (WaveFrame+1)%4
            DEFINE 6,1,VARPTR Title_wave(WaveFrame * 4)
        END IF
        WAIT
    WEND
    Debounce = DEBOUNCE_DELAY
    END

INFO_HELP_SCREEN: PROCEDURE
    PlaySnd(InputSound)
    CLS
    ResetSprite(1)
    ResetSprite(2)
    ResetSprite(3)
    SCREEN Title_cards,0,0,8,1,8
    SPRITE 4,  8 + VISIBLE + ZOOMX2, 11 , SPR06 + BEHIND + SPR_ORANGE
    SPRITE 5, 24 + VISIBLE + ZOOMX2, 11 , SPR06 + BEHIND + SPR_ORANGE
    SPRITE 6, 40 + VISIBLE + ZOOMX2, 11 , SPR06 + BEHIND + SPR_ORANGE
    SPRITE 7, 56 + VISIBLE + ZOOMX2, 11 , SPR06 + BEHIND + SPR_ORANGE
    PRINT AT SCREENPOS(11,0) COLOR CS_YELLOW, "Info help"
    PRINT AT SCREENPOS(1,4) COLOR CS_TAN, "8 or UP      Go Up"
    PRINT AT SCREENPOS(1,5) COLOR CS_TAN, "0 or DN      Go Dn"
    PRINT AT SCREENPOS(1,8) COLOR CS_TAN, "CLEAR         EXIT"
    PRINT AT SCREENPOS(3,11) COLOR CS_YELLOW, "<CLR>  to exit"
    WHILE (CONT <> $88)  'CLEAR
        IF FRAME%8 = 0 THEN
            WaveFrame = (WaveFrame+1)%4
            DEFINE 6,1,VARPTR Title_wave(WaveFrame * 4)
        END IF
        WAIT
    WEND
    Debounce = DEBOUNCE_DELAY
    ' restore HELP display
    PRINT AT SCREENPOS(11,0) COLOR CS_YELLOW, "         "
    SCREEN Help_Cards,0,17,3,1,3
    END

DISP_INFO: PROCEDURE
    ' Display current information screen
    #f_from  = PI_GET_INFO_DISP 
    #f_total = PI_GET_INFO_NUM
    #f_to    = PI_GET_INFO_DISP+1
    GOSUB DISPLAY_SLIDER
	#Disp_Color = CS_WHITE
	
    FOR J=0 TO 9
        FOR I=0 TO INFO_LENGTH - 1
            #tmp = PEEK((ADDRESS_INFO_PAGE+I)+INFO_LENGTH*J)
            IF #tmp = 255 THEN #tmp = 0
			IF (I + J = 0) THEN #Disp_Color = CS_WHITE + $2000 ELSE #Disp_Color = CS_WHITE
            #BACKTAB(40 + J*20 + I) = ASCII_table(#tmp) + #Disp_Color
        NEXT I  
    NEXT J
	
	' Display section in title bar
    #tmp = PEEK(ADDRESS_section) AND $7F
    ' First letter need to change Color Stack
    #BACKTAB(20) = ASCII_table(#tmp) + CS_GREEN + $2000
    FOR I = 1 TO 19
        #tmp = PEEK(ADDRESS_section + I) AND $7F
        #BACKTAB(20+I) = ASCII_table(#tmp) + CS_GREEN
    NEXT I
	
    END

INFO_SCREEN: PROCEDURE
    PlaySnd(InputSound)
    IF PI_GET_FTYPE(Selected_Entry) <> TYPE_DIR THEN
        PI_SELECTENTRY(Selected_Entry)
        PI_CMD(CMD_READINFO)
        GOSUB WAIT_CARD_ANSWER
        IF PI_GET_INFO_NUM > 0 THEN
            ' Info available for this entry, display it
            GOSUB DISP_INFO
            Input = 0
            WHILE (Input <> $88)  ' run until CLR key is pressed
                IF Debounce>0 THEN 
                    Debounce = Debounce-1
                ELSE
                    ' Get new input and process it
                    Input = CONT
                    IF (Input=$48 OR Input=$01 OR Input=$11 OR Input=$19 OR Input=$09 OR Input=$03 OR Input=$13 OR Input=$12) THEN
                        ' Display next info page
                        PlaySnd(InputSound)
                        Debounce = DEBOUNCE_DELAY
                        PI_CMD(CMD_NEXTINFO)
                        GOSUB WAIT_CARD_ANSWER
                        GOSUB DISP_INFO
                    END IF
                    IF (Input=$44 or Input=$04 or Input=$0C or Input=$1C or Input=$18 or Input=$14 or Input=$16 or Input=$06) THEN
                        ' Display prev info page
                        PlaySnd(InputSound)
                        Debounce = DEBOUNCE_DELAY
                        PI_CMD(CMD_PREVINFO)
                        GOSUB WAIT_CARD_ANSWER
                        GOSUB DISP_INFO
                    END IF
                    IF (Input=$81) THEN
                        Debounce = DEBOUNCE_DELAY
                        GOSUB INFO_HELP_SCREEN
                        GOSUB DISP_INFO
                    END IF                    
                END IF
                ' Update animation frame
                IF FRAME%8 = 0 THEN
                    WaveFrame = (WaveFrame+1)%4
                    DEFINE 6,1,VARPTR Title_wave(WaveFrame * 4)
                END IF
                WAIT
            WEND
        END IF
    END IF
    Debounce = DEBOUNCE_DELAY
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
            
            ' If error is file to big, display error message and wait for user to press clear
            IF PI_GET_ERROR = ERR_FILE_TO_BIG THEN
                PRINT AT SCREENPOS(0,4)  COLOR CS_RED, "                   "
                PRINT AT SCREENPOS(0,5)  COLOR CS_RED, "   File too big!   "
                PRINT AT SCREENPOS(0,6)  COLOR CS_RED, "                   "
                PRINT AT SCREENPOS(0,10) COLOR CS_RED, "                   "
                PRINT AT SCREENPOS(0,11) COLOR CS_RED, "   CLR to return   "
                WHILE (CONT <> $88)  'CLEAR
                    WAIT
                WEND
            ELSEIF PI_GET_ERROR = ERR_COULD_NOT_OPEN_FILE THEN
                PRINT AT SCREENPOS(0,4)  COLOR CS_RED, "                   "
                PRINT AT SCREENPOS(0,5)  COLOR CS_RED, "Couldn't open file!"
                PRINT AT SCREENPOS(0,6)  COLOR CS_RED, "                   "
                PRINT AT SCREENPOS(0,10) COLOR CS_RED, "                   "
                PRINT AT SCREENPOS(0,11) COLOR CS_RED, "   CLR to return   "
                WHILE (CONT <> $88)  'CLEAR
                    WAIT
                WEND
            ELSE
                ' Infinite loop till getting reseted by card
                ASM InfiniteLoop:
                ASM    B InfiniteLoop
            END IF
        END IF
    END IF
    END

UP_DIRECTORY: PROCEDURE
    PI_CMD(CMD_UPDIRECTORY)
    PlaySnd(InputSound)
    GOSUB WAIT_CARD_ANSWER
    Selected_Entry = 0
    END

WAIT_CARD_ANSWER: PROCEDURE
    I = 0
    WHILE PI_STATUS = PI_STAT_BUZZY
        DEFINE 63,1,VARPTR In_Progress(I * 4)
        WAIT
        SPRITE 0, 84 + VISIBLE, 56 + ZOOMY2, SPR63 + SPR_RED
        I = (I+1)
        IF I=20 THEN I=0
        WAIT
    WEND
	ResetSprite(0)
    END

DISPLAY_FILELIST: PROCEDURE
    Max_Entry = #f_to - #f_from
    SelEnt_Start = 0
    SelEnt_Length = 0

    IF Max_Entry > 0 THEN 
        Max_Entry = Max_Entry - 1
        FOR J=0 TO Max_Entry
            ' First icon needs also to change colorstack
            IF J = 0 THEN #tmp = $2000 else #tmp = 0
            IF PI_GET_FTYPE(J) = TYPE_DIR THEN
                #Disp_Color = CS_DARKGREEN
                #BACKTAB(J*20 + 40) = BG00 + CS_YELLOW + #tmp
            ELSE
                #Disp_Color = CS_TAN
                #BACKTAB(J*20 + 40) = BG01 + CS_CYAN + #tmp
            END IF
            IF J = Selected_Entry THEN #Disp_Color = CS_GREEN
            FOR I=0 TO 17
                #tmp = PEEK((ADDRESS_flist+I)+FNAME_LENGTH*J)
                IF #tmp = 255 THEN #tmp = 0
                #BACKTAB(41 + J*20 + I) = ASCII_table(#tmp) + #Disp_Color
            NEXT I
        NEXT J
		WHILE ((PEEK((ADDRESS_flist+SelEnt_Length)+FNAME_LENGTH*Selected_Entry)<>255) AND (SelEnt_Length<FNAME_LENGTH))
			SelEnt_Length = SelEnt_Length + 1 
		WEND
	ELSE
        ' Change background color
        #BACKTAB(40) = $2000
        IF ((PI_CURRENTDEVICE = DEV_SD) AND (PI_SD_PRESENT = 0)) THEN
            ' No SD Card present => Display <NO SD> in middle of screen
            #BACKTAB(126) = ASCII_table(28)+CS_WHITE
            #BACKTAB(127) = ASCII_table(46)+CS_WHITE
            #BACKTAB(128) = ASCII_table(47)+CS_WHITE
            #BACKTAB(129) = ASCII_table(00)+CS_WHITE
            #BACKTAB(130) = ASCII_table(51)+CS_WHITE
            #BACKTAB(131) = ASCII_table(36)+CS_WHITE
            #BACKTAB(132) = ASCII_table(30)+CS_WHITE
        ELSE
            ' Empty Directory => Display <EMPTY> in middle of screen
            #BACKTAB(126) = ASCII_table(28)+CS_WHITE
            #BACKTAB(127) = ASCII_table(37)+CS_WHITE
            #BACKTAB(128) = ASCII_table(45)+CS_WHITE
            #BACKTAB(129) = ASCII_table(48)+CS_WHITE
            #BACKTAB(130) = ASCII_table(52)+CS_WHITE
            #BACKTAB(131) = ASCII_table(57)+CS_WHITE
            #BACKTAB(132) = ASCII_table(30)+CS_WHITE
        END IF
    END IF
    END

DISPLAY_SELECTED_ENTRY: PROCEDURE
    ' Scroll selected entry display if it doesn't fit on screen
    IF SelEnt_Length > 18 THEN
        SelEnt_Start = SelEnt_Start + 1
        IF SelEnt_Start > SelEnt_Length THEN SelEnt_Start = 0
        FOR I = 0 TO 17
            #tmp = SelEnt_Start + I
            IF #tmp = SelEnt_Length THEN
                #BACKTAB(41 + Selected_Entry*20 + I) = ASCII_table(0) + CS_GREEN
            ELSE
                IF #tmp > SelEnt_Length THEN #tmp = #tmp - SelEnt_Length -1
                #tmp = PEEK((ADDRESS_flist+#tmp)+FNAME_LENGTH*Selected_Entry)
                #BACKTAB(41 + Selected_Entry*20 + I) = ASCII_table(#tmp) + CS_GREEN
            END IF
        NEXT I
    END IF
    END

DISPLAY_SLIDER: PROCEDURE
    IF #f_from < #f_to THEN ' Not an Empty list
        I = ((79 * #f_from) / #f_total)
        J = ((79 * #f_to)   / #f_total)
        IF I > 71   THEN I = 71
        IF J < I+7  THEN J = I+7
        K = J-I

        IF K > 64 THEN
            SPRITE 3, 160 + VISIBLE, 24      + I + ZOOMY8 + DOUBLEY, SPR02 + SPR_GREY
            SPRITE 2, 160 + VISIBLE, 24 - 64 + J + ZOOMY8 + DOUBLEY, SPR02 + SPR_GREY
        ELSEIF K > 32 THEN
            SPRITE 3, 160 + VISIBLE, 24       + I + ZOOMY8, SPR02 + SPR_GREY
            SPRITE 2, 160 + VISIBLE, 24  - 32 + J + ZOOMY8, SPR02 + SPR_GREY
        ELSEIF K > 16 THEN
            SPRITE 3, 160 + VISIBLE, 24      + I + ZOOMY4, SPR02 + SPR_GREY
            SPRITE 2, 160 + VISIBLE, 24 - 16 + J + ZOOMY4, SPR02 + SPR_GREY
        ELSE
            SPRITE 3, 160 + VISIBLE, 24     + I + ZOOMY2, SPR02 + SPR_GREY
            SPRITE 2, 160 + VISIBLE, 24 - 8 + J + ZOOMY2, SPR02 + SPR_GREY
        END IF

        SPRITE 1, 160 + VISIBLE, 24 - 4 + (I+J) / 2 + ZOOMY2,  $20E8 'Using "=" character from GROM
    ELSE
        ' Empty list => Display empty slider
        ResetSprite(1)
        ResetSprite(2)
        ResetSprite(3)
    END IF

    FOR I=59 TO 219 STEP 20:#BACKTAB(I)=$0827:NEXT I
    #BACKTAB(239) = $082F
    END

'test_init: PROCEDURE
'    K = 0
'    
'    FOR J=0 TO 9
'        POKE(ADDRESS_ftype+K),TYPE_FILE
'        FOR I = 0 TO FNAME_LENGTH-4*J
'            POKE(ADDRESS_flist+I + FNAME_LENGTH*J),K+16
'            K = K+1
'            IF K = 10 THEN K = 0
'        NEXT I
'        POKE(ADDRESS_flist+I + FNAME_LENGTH*J),255
'    NEXT J
'
'    POKE(ADDRESS_ftype+0),TYPE_DIR
'    POKE(ADDRESS_ftype+1),TYPE_DIR
'    POKE(ADDRESS_ftype+2),TYPE_DIR
'
'    POKE(ADDRESS_path+0), 15
'    POKE(ADDRESS_path+1), 51
'    POKE(ADDRESS_path+2), 36
'    POKE(ADDRESS_path+3), 15
'    POKE(ADDRESS_path+4), 36
'    POKE(ADDRESS_path+5), 41
'    POKE(ADDRESS_path+6), 50
'    POKE(ADDRESS_path+7), 17
'    POKE(ADDRESS_path+8), 15
'    POKE(ADDRESS_path+9), 36
'    POKE(ADDRESS_path+10),41
'    POKE(ADDRESS_path+11),50
'    POKE(ADDRESS_path+12),18
'    
'    POKE(ADDRESS_ffrom),0
'    POKE(ADDRESS_ffrom+1),0
'    POKE(ADDRESS_fto),0
'    POKE(ADDRESS_fto+1),10
'    POKE(ADDRESS_ftotal),0
'    POKE(ADDRESS_ftotal+1),32
'
'    FOR J=0 TO 9
'        FOR I = 0 TO INFO_LENGTH - 1
'            POKE(ADDRESS_INFO_PAGE + I + J * INFO_LENGTH),41
'        NEXT I
'    NEXT J
'	POKE (ADDRESS_INFO_DISP),0
'	POKE (ADDRESS_INFO_NUM) ,9
'
'    END

'
' DATA
'
    INCLUDE "Minty_logo.bas"
    INCLUDE "text.bas"
    INCLUDE "Sounds.bas"
    INCLUDE "font.bas"

Icons:
    DATA $E000,$E2FE,$8282,$00FE
DATA $3800,$D644,$54CE,$0038
'    DATA $4438,$CED6,$54D6,$0038

Slider:
    DATA $7C7C,$7C7C,$7C7C,$7C7C
    DATA $7C7C,$7C7C,$7C7C,$7C7C
    DATA $8282,$8282,$8282,$8282
    DATA $8282,$8282,$8282,$FE82

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

Help_text:
	DATA $1808,$080A,$000A,$0000
	DATA $A8AE,$A8EC,$00AE,$0000
	DATA $8A8E,$888E,$00E8,$0000

Help_cards:
	DATA $1872,$187A,$1882
