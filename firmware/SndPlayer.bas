	DIM #SndAdd, #SndCmd
	DEF FN PlaySnd(SndName) = #SndAdd = VARPTR SndName(0)
	ON FRAME GOSUB PlaySndFn
	GOTO EndSnd

PlaySndFn: PROCEDURE
	IF (#SndAdd <> $0000) THEN
		#SndCmd = PEEK(#SndAdd)
		IF (#SndCmd = $0000) THEN
			SOUND 2,100,0
			SOUND 4,,$38
			#SndAdd = $0000
		ELSE
			SOUND 2, #SndCmd/16, #SndCmd%16
			#SndAdd = #SndAdd+1
			#SndCmd = PEEK(#SndAdd)
			SOUND 4, #SndCmd/256, #SndCmd%256
			#SndAdd = #SndAdd+1
		END IF
	END IF
	END
EndSnd:
