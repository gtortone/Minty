    ASM MEMATTR $8000, $9fff, "+RWN"

    INCLUDE "constants.bas"
    INCLUDE "splash.bas"
    CONST mfile=$817f
    CONST mst=$813f
    const riga=$8899
    const cmd=$8889
    const done=$8119
    const hack=$9111
    const dirfile=$8651
    const chk=$815e
    const dev=$8120

    dim tipo(10)
    poke(mst),0
    poke(chk),0
    poke(dirfile),0

    GOSUB reset_sound

    for A=1 to 2    
        IF A=1 THEN #C=477
        IF A=2 THEN #C=239
    SOUND 0,#C,PSG_ENVELOPE_ENABLE
    SOUND 1,(#C+1)/2,PSG_ENVELOPE_ENABLE
    SOUND 2,#C*2,PSG_ENVELOPE_ENABLE
    SOUND 3,6000,PSG_ENVELOPE_SINGLE_SHOT_RAMP_DOWN_AND_OFF ' Slow decay, single shot \______
    FOR C = 1 TO 30:WAIT:NEXT C
    NEXT A

    GOSUB reset_sound

    #cnt=0
    WHILE (cont = NO_KEY) and (peek(done)<>123) and (#cnt<100) ' 0x119)  
      #cnt=#cnt+1
      WAIT
    WEND
    poke(chk),0
    WAIT
   
    poke(cmd),0

    pgup_state = 0

avanti:
    
    curriga=0
    cls

    PRINT AT SCREENPOS(7, 0) COLOR CS_GREEN, " Minty"
    #from=0
    #f_from=(peek($9028) * 256) + peek($9029)
    #f_to=(peek($9030) * 256) + peek($9031)
    #f_total=(peek($9032) * 256) + peek($9033)
    if peek(dev)=0 then
       PRINT AT SCREENPOS(0, 11) COLOR CS_WHITE, "FL:"
    else
       PRINT AT SCREENPOS(0, 11) COLOR CS_WHITE, "SD:"
    end if
    if #f_from = #f_to then ' empty list
      #from=0
    else
      #from=#f_from+1
    end if

    PRINT AT SCREENPOS(3, 11) COLOR CS_WHITE, <3>#from, "-", <3>#f_to, "/", <3>#f_total, " 1:HLP"
      
menu:
    GOSUB leggimenu

    WAIT
    c=cont

    'SELECT
    if (c=40) then   'ENTER
        if #f_from < #f_to then
           k=0
           CLS
           print at screenpos(3,2) color CS_TAN," Loading" ' root
           print at screenpos(3,5),"Please wait..."
           
           poke(riga),curriga+1      
           'poke(done),0
           poke(cmd),2
           
           while peek(done)<>1
               if k=0 then print at screenpos(10,8),BG28 + CS_BLUE
               if k=1 then print at screenpos(10,8),BG29 + CS_BLUE
               if k=2 then print at screenpos(10,8),BG30 + CS_BLUE
               if k=3 then print at screenpos(10,8),BG31 + CS_BLUE
               k=k+1
               if k>3 then k=0
           wend

           poke(cmd),0  
           if tipo(curriga)=2 then goto avanti
           ' game
           cls
           print at screenpos(3,2) color CS_TAN," Loading game" ' root
           print at screenpos(3,5),"Please wait..."
           goto fine
        end if
    end if

    'UPDIR
    if (c=136) then  'CLEAR
        sound 0,120,15
        for p=0 to 5:next p
        sound 0,0,0 
        cls
        k=0
        poke(cmd),5
        while peek(done)<>1 
            print at screenpos(3,2) color CS_TAN,"Up directory"
            print at screenpos(3,5),"Please wait..."
            if k=0 then print at screenpos(10,8),BG28 + CS_BLUE
            if k=1 then print at screenpos(10,8),BG29 + CS_BLUE
            if k=2 then print at screenpos(10,8),BG30 + CS_BLUE
            if k=3 then print at screenpos(10,8),BG31 + CS_BLUE
            k=k+1
            if k>3 then k=0   
        wend
        
        poke (cmd),0  
        goto avanti
    end if    
    
    'HELP
    if (c=129) then     'KEYPAD_1
      cls
      PRINT at SCREENPOS(8,0) COLOR CS_GREEN, "HELP"
      PRINT AT SCREENPOS(0,2) COLOR CS_WHITE, "8 or up:     go up"
      PRINT AT SCREENPOS(0,3) COLOR CS_WHITE, "0 or down:   go dn"
      PRINT AT SCREENPOS(0,4) COLOR CS_WHITE, "7 or btn-up: page up"
      PRINT AT SCREENPOS(0,5) COLOR CS_WHITE, "9 or btn-dn: page dn"
      PRINT AT SCREENPOS(0,7) COLOR CS_WHITE, "CLEAR:       dir up"
      PRINT AT SCREENPOS(0,8) COLOR CS_WHITE, "ENTER:       select"
      PRINT AT SCREENPOS(0,11) COLOR CS_YELLOW, "  <CLEAR> to exit"

      WAIT
      WHILE (cont<>136)  'CLEAR
         WAIT
      WEND
      goto avanti
    end if

    'DOWN
    if ((c=72 or c=1 or c=17 or c=25 or c=9 or c=3 or c=19 or c=18) and curriga<=lastriga) then      'KEYPAD_0 or S/SE/SW
       if curriga=lastriga then goto nextpage
       curriga=curriga+1
       sound 0,140,15
       for p=0 to 9:next p
       wait
       sound 0,0,0 
    end if
    
    'UP
    if ((c=68 or c=4 or c=12 or c=28 or c=24 or c=20 or c=22 or c=6) and curriga>=0) then    'KEYPAD_8 or N/NNW/NNE
       if (curriga=0 and #f_from>0) then 
          pgup_state = 1
          goto prevpage
       end if
       if curriga>0 then
          curriga=curriga-1 
          sound 0,140,15
          for p=0 to 9:next p
          wait
          sound 0,0,0 
       end if
    end if

    'PGDOWN
    if c=96 or c=192 or c=36 then     'b-left or b-right or KEYPAD_9
nextpage:
      if #f_to < #f_total then
        sound 0,140,15
        sound 0,0,0 
        k=0
        cls
        print at screenpos(3,2) color CS_TAN,"Loading next page"
        print at screenpos(3,5),"Please wait..."
        poke (cmd),3
        while peek(done)<>1
            if k=0 then print at screenpos(10,8),BG28 + CS_BLUE
            if k=1 then print at screenpos(10,8),BG29 + CS_BLUE
            if k=2 then print at screenpos(10,8),BG30 + CS_BLUE
            if k=3 then print at screenpos(10,8),BG31 + CS_BLUE    
            k=k+1
            if k>3 then k=0
            'poke(cmd),3
        wend
        poke (cmd),0
        goto avanti
      end if
    end if
 
    'PGUP
    if c=160 or c=132 then    'b-top or KEYPAD_7
prevpage:
      if #f_from > 0 then
        sound 0,120,15
        for p=0 to 9:next p
        wait
        sound 0,0,0 
        k=0
        cls
        print at screenpos(3,2) color CS_TAN,"Loading prev page"
        print at screenpos(3,5),"Please wait..."
        poke(cmd),4
        while peek(done)<>1
            if k=0 then print at screenpos(10,8),BG28 + CS_BLUE
            if k=1 then print at screenpos(10,8),BG29 + CS_BLUE
            if k=2 then print at screenpos(10,8),BG30 + CS_BLUE
            if k=3 then print at screenpos(10,8),BG31 + CS_BLUE
            k=k+1
            if k>3 then k=0
            'poke(cmd),4
        wend
        poke(cmd),0  
        goto avanti
      end if
    end if

    'CHANGE STORAGE DEVICE
    if c=33 then     ' KEYPAD_3
      sound 0,120,15
      for p=0 to 9:next p
      wait
      sound 0,0,0 
      k=0
      cls
      if peek(dev) = 1 then
         poke(dev),0    'switch to flash
      else
         poke(dev),1    'switch to SD
      end if
      print at screenpos(0,2) color CS_TAN,"Mount storage device"
      print at screenpos(3,5),"Please wait..."
      poke(cmd),6
      while peek(done)<>1
          if k=0 then print at screenpos(10,8),BG28 + CS_BLUE
          if k=1 then print at screenpos(10,8),BG29 + CS_BLUE
          if k=2 then print at screenpos(10,8),BG30 + CS_BLUE
          if k=3 then print at screenpos(10,8),BG31 + CS_BLUE
          k=k+1
          if k>3 then k=0
      wend
      poke(cmd),0
      goto avanti
    end if

    goto menu
   
fine:
    goto fine

'
' PROCEDURE
'

leggimenu: PROCEDURE
    lastriga=0
    emptylines=0
    for j=0 to 9
        lastriga=lastriga+1
        for i=0 to 19
            #mem=peek((mfile+i*2)+40*j)
            if i=0 and #mem=0 then ' empty line
                #mem=32
                emptylines=emptylines+1
                tipo(j)=0
            end if 

            if peek($9000+j)=1 then 
                tipo(j)=2 'dir
            else
                tipo(j)=1 'file
            end if

            if pgup_state=0 then
               if j=curriga then 
                   if #mem<32 then #mem=32
                   PRINT AT screenpos(i,j+1),(#mem-32)*8+CS_GREEN
               else  
                  if peek($9000+j)=1 then
                      if #mem<32 then #mem=32
                      PRINT AT screenpos(i,j+1), (#mem-32)*8+CS_BLUE
                  else
                      if #mem<32 then #mem=32
                      PRINT AT screenpos(i,j+1), (#mem-32)*8+CS_TAN
                  end if  
               end if
            end if
        next i
    next j
    lastriga=lastriga-emptylines-1

    if pgup_state=1 then 'highlight last row
      curriga=lastriga
      for i=0 to 19
         PRINT AT screenpos(i,curriga+1),(#mem-32)*8+CS_YELLOW
      next i
      pgup_state=0
    end if
  END

  ' 32 bitmaps
    
reset_sound:    PROCEDURE
    SOUND 0,1,0
    SOUND 1,1,0
    SOUND 2,1,0
    SOUND 4,,$38
    RETURN
    END

'
' DATA
'

screen_bitmaps_0:
    DATA $0000,$0000,$3F00,$FF7F
    DATA $0000,$0000,$FF00,$FFFF
    DATA $0000,$0000,$0301,$0303
    DATA $0000,$0000,$FFFF,$F1FF
    DATA $0000,$0000,$F180,$FCF9
    DATA $0000,$0000,$FFFF,$1FFF
    DATA $0000,$0000,$FCFE,$80FC
    DATA $0000,$0000,$1F03,$F87F
    DATA $0000,$0000,$F0C0,$FCF8
    DATA $8CCC,$0C0C,$1C0C,$3C1C
    DATA $7070,$7070,$7070,$7170
    DATA $0703,$0707,$0F0F,$0F0F
    DATA $E1E0,$FFE3,$FFFF,$FFFF
    DATA $F8FC,$F0F8,$00E0,$8000
    DATA $1F1F,$3F1F,$3E3F,$7E3E
    DATA $0381,$0703,$0707,$0707
screen_bitmaps_1:
    DATA $F0F0,$E0F0,$E0E0,$C1C0
    DATA $7C7C,$FC7C,$FCFC,$F8F8
    DATA $7838,$3078,$0000,$0000
    DATA $7673,$1C3E,$0000,$0000
    DATA $1F1F,$3F1F,$003F,$0000
    DATA $9F9F,$8F8F,$0007,$0000
    DATA $C080,$E0E0,$00F0,$0000
    DATA $7C7C,$FC7C,$00F8,$0000
    DATA $0707,$0103,$0000,$0000
    DATA $E3C1,$FFFF,$0078,$0000
    DATA $E0F0,$00C0,$0000,$0000

    ' Real Copyright Symbol
    BITMAP ".######."
    BITMAP "#......#"
    BITMAP "#..###.#"
    BITMAP "#.#....#"
    BITMAP "#.#....#"
    BITMAP "#..###.#"
    BITMAP "#......#"
    BITMAP ".######."

BITMAP "....##.."
BITMAP "......#."
BITMAP ".......#"
BITMAP ".......#"
BITMAP "........"
BITMAP "........"
BITMAP "........"
BITMAP "........"

BITMAP "........"
BITMAP "........"
BITMAP "........"
BITMAP "........"
BITMAP ".......#"
BITMAP ".......#"
BITMAP "......#."
BITMAP "....##.."

BITMAP "........"
BITMAP "........"
BITMAP "........"
BITMAP "........"
BITMAP "#......."
BITMAP "#......."
BITMAP ".#......"
BITMAP "..##...."

BITMAP "..##...."
BITMAP ".#......"
BITMAP "#......."
BITMAP "#......."
BITMAP "........"
BITMAP "........"
BITMAP "........"
BITMAP "........"

    REM 20x12 cards
screen_cards:
    DATA $0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000
    DATA $0000,$0000,$0000,$0000,$0000,$0802,$080A,$0000,$0812,$081A,$0822,$082A,$0832,$083A,$0842,$0000,$0000,$0000,$0000,$0000
    DATA $0000,$0000,$0000,$0000,$0000,$084A,$0852,$0000,$085A,$0862,$086A,$0872,$087A,$0882,$088A,$0000,$0000,$0000,$0000,$0000
    DATA $0000,$0000,$0000,$0000,$0000,$0892,$089A,$0000,$08A2,$08AA,$08B2,$08BA,$08C2,$08CA,$08D2,$0000,$0000,$0000,$0000,$0000
    DATA $0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000
    DATA $0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000
    DATA $0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000
    DATA $0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000
    DATA $0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000
    DATA $0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000
    DATA $0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000
    DATA $0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000,$0000
