'' Phantomas Lhikoj
''
'' un remake de un juego perdido de LOKOsoft (1991/1992).
''
'' uses Suvleir 3.0 and fourSpriter 2.1
'' uses a modified monk sprite by Anjuel
''

#include once "lhikojSpriteSet.bas"
#include once "lhikojCharSet.bas"
#include once "fsp2.1.bas"
#include once "aplib.bas"
#include once "gfxParser.bas"
#include once "pantallas.bas"

'' Variables
Dim plX As uByte
Dim plY As uByte
Dim plStep As uByte
Dim plFacing As uByte
Dim plCanFire As uByte

Dim icePlX As uByte
Dim icePlX2 As uByte
Dim icePlY As uByte
Dim icePlY2 As uByte
Dim icePlSal As uByte
Dim icePlSal2 As uByte
Dim icePlNu As uByte
Dim icePlNu2 As uByte
Dim icePlFacing As uByte
Dim icePlStep As uByte
Dim plObonabilobos As uByte

Dim i As uByte
Dim j As uByte
Dim k As uByte
Dim ii As uInteger
Dim idx As uInteger
Dim i2dx As uInteger
Dim jdx As uInteger
Dim j2dx As uInteger
Dim nPant As uInteger

Dim obonabilobos (3) As uByte

'' Linebuffer.
linebuffer:
Asm
  defb 0, 1, 2, 3, 4, 5, 6, 7, 64, 65, 66, 67, 68, 69, 70, 71, 128, 129, 130, 131, 132, 133, 134, 135
End Asm

'' Connections
'' Four connections per screen.
'' X1, Y1, X2, Y2, screen, X, Y
Dim connections (251) As uByte => { _
	13, 22, 16, 22, 1, -1, 1, _
	0,0,0,0,0,0,0, _
	0,0,0,0,0,0,0, _
	0,0,0,0,0,0,0, _
	_
	13, 0, 16, 0, 0, -1, 21, _
	30, 9, 30, 10, 2, 1, -1, _
	30, 17, 30, 18, 2, 1, -1, _
	11, 22, 15, 22, 3, -1, 1, _
	_
	0, 9, 0, 10, 1, 29, -1, _
	0, 17, 0, 18, 1, 29, -1, _
	0,0,0,0,0,0,0, _
	0,0,0,0,0,0,0, _
	_
	11, 0, 15, 0, 1, -1, 21, _
	0,0,0,0,0,0,0, _
	0,0,0,0,0,0,0, _
	0,0,0,0,0,0,0, _
	_
	30,7,30,22,5,1,-1, _
	10,22,30,22,7,-1,1, _
	0,0,0,0,0,0,0, _
	0,0,0,0,0,0,0, _
	_
	0,7,0,22,4,29,-1, _
	30,1,30,4,8,23,1, _
	0,22,5,22,6,-1,1, _
	0,0,0,0,0,0,0, _
	_
	0,0,5,0,5,-1,21, _
	0,1,0,17,7,29,-1, _
	30,2,30,22,8,1,-1, _
	0,0,0,0,0,0,0, _
	_
	10,0,30,0,4,-1,21, _
	30,1,30,17,6,1,-1, _
	2,18,2,18,7,1,3, _
	1,4,1,4,7,2,19, _
	_
	0,2,0,22,6,29,-1, _
	21,0,26,0,5,29,2, _
	0,0,0,0,0,0,0, _
	0,0,0,0,0,0,0 _	
}

'' Hotspots
'' Four hotspots per screen
'' X1, Y1, X2, Y2, type, aux
Dim hotspots (215) As uByte => { _
	0,0,0,0,0,0, _
	0,0,0,0,0,0, _
	0,0,0,0,0,0, _
	0,0,0,0,0,0, _
	_
	4, 12, 4, 12, 1, 0, _
	13, 17, 14, 17, 1, 1, _
	0,0,0,0,0,0, _
	0,0,0,0,0,0, _
	_
	12, 13, 19, 13, 1, 2, _
	22, 7, 28, 11, 2, 0, _
	0,0,0,0,0,0, _
	0,0,0,0,0,0, _
	_
	19, 6, 22, 6, 1, 3, _
	0,0,0,0,0,0, _
	0,0,0,0,0,0, _
	0,0,0,0,0,0, _
	_
	0,0,0,0,0,0, _
	0,0,0,0,0,0, _
	0,0,0,0,0,0, _
	0,0,0,0,0,0, _
	_
	0,0,0,0,0,0, _
	0,0,0,0,0,0, _
	0,0,0,0,0,0, _
	0,0,0,0,0,0, _
	_
	0,0,0,0,0,0, _
	0,0,0,0,0,0, _
	0,0,0,0,0,0, _
	0,0,0,0,0,0, _
	_
	0,0,0,0,0,0, _
	0,0,0,0,0,0, _
	0,0,0,0,0,0, _
	0,0,0,0,0,0, _
	_
	0,0,0,0,0,0, _
	0,0,0,0,0,0, _
	0,0,0,0,0,0, _
	0,0,0,0,0,0 _
}

Dim flags (20) As uByte

#include once "iceLevels.bas"

'' Start

iceLevels (0) = @iceLevel0
iceLevels (1) = @iceLevel1
iceLevels (2) = @iceLevel2
iceLevels (3) = @iceLevel3

Paper 7: Ink 0: Bright 0: Border 0: Over 0: Inverse 0: Bold 1: Italic 1: Flash 0: Cls

fsp21SetGfxAddress (@lhikojSpriteSet (0))		' Sets fourspriter to draw chars from lhikojSpriteSet

theGame ()

Sub initPlayer ()
	plX = 2
	plY = 4
	plStep = 0
	plFacing = 0
	plCanFire = 0
End Sub

Sub initWorld (n As uByte)
	nPant = n
	idx = nPant * 28
	i2dx = nPant * 24
	plX = 4
	plY = 4
	fsp21MoveSprite (3, plX, plY)
	fsp21DuplicateCoordinatesSprite (3)
	fsp21ActivateSprite (3)
	displayScreen (nPant)
	displayExtra (nPant)
	fsp21InitSprites ()
End Sub

Sub theGame ()
	Dim doMyGame As uByte
	Dim chgScr As uByte

	'' Presentation

	'' Endless loop ahead:
		
	doMyGame = 1
	While (doMyGame)
		'' Init everything (Game starts here)
	
		nPant = 0
		'''nPant = 4
		idx = nPant * 28
		i2dx = nPant * 24
		
		For i = 0 To 3: obonabilobos (i) = 0: Next i
		For i = 0 To 20: flags (i) = 99: Next i
		plObonabilobos = 0

		''

		fsp21ActivateSprite (3)

		'' First cutscene
		intro (0)
				
		'' Init
		initPlayer ()
		initWorld (nPant)
		
		finished = 0
		While Not finished

			playerMove ()
			
			Asm
				halt
				halt
			End Asm
			'if inkey="i"Then plObonabilobos = plObonabilobos + 1: While Inkey="i":Wend:End If
			fsp21UpdateSprites ()
			
			'Print At 21,0;plX;",";plY;"   "
			
			If flags (1) = 1 Then Print At 0,28; Inverse 1; "OB:"; plObonabilobos: End If
			
			jdx = idx
			j2dx = i2dx
			For ii = 0 To 3
				'' Connections
				If plX >= connections (jdx) And plX <= connections (jdx + 2) And _
				   plY >= connections (jdx + 1) And plY <= connections (jdx + 3) Then
					If nPant = connections (jdx + 4) Then chgScr = 0: Else chgScr = Not 0: End If
				   	nPant = connections (jdx + 4)
					idx = nPant * 28
					i2dx = nPant * 24
					If connections (jdx + 5) <> -1 Then plX = connections (jdx + 5): End If
					If connections (jdx + 6) <> -1 Then plY = connections (jdx + 6): End If
					If chgScr Then
						fsp21MoveSprite (3, plX, plY)
						fsp21DuplicateCoordinatesSprite (3)
						displayScreen (nPant)
						displayExtra (nPant)
						fsp21InitSprites ()
					End If
					Exit For
				End If
				jdx = jdx + 7
				
				'' Hotspots
				If plX >= hotspots (j2dx) And plX <= hotspots (j2dx + 2) And _
				   plY >= hotspots (j2dx + 1) And plY <= hotspots (j2dx + 3) Then
					If hotspots (j2dx + 4) = 1 Then
						' Get Obonabilobo! Flag = 1
						If flags (1) = 1 Then
							iceGame (hotspots (j2dx + 5))
							fsp21ActivateSprite (3)
							displayScreen (nPant)
							displayExtra (nPant)
						Else
							SaveScreen ()
							scrollMsg ("SADLY...", " ", "THE DOOR IS CLOSED.")
							waitKey ()
							RestoreScreen ()
						End If
						plY = plY + 1
					ElseIf hotspots (j2dx + 4) = 2 Then
						k = 0
						SaveScreen ()
						If flags (1) <> 1 Then
							flags (1) = 1
							scrollMsg ("LE VIELHO SAYS", "BRING ME 4 OBONABILOBOS", "I OPENED THE ICE CAVES")
						Else
							If plObonabilobos < 4 Then
								scrollMsg ("LE VIELHO SAYS", "'I ALREADY TOLD YOU", "WHAT TO DO'")
							Else
								'' Message and move to screen 5.
								k = 1
								scrollMsg ("LE VIELHO SAYS", "'OH! MY OBONABILOBOS", "THANK YOU'")
								waitKey ()
								scrollMsg ("LE VIELHO SAYS", "'I HAVE A PRESENT:", "THE KILLING SPELL!'")
								waitKey ()
								scrollMsg ("LE VIELHO SAYS", "'USE IT WISELY...", "PREPARE TO TRANSPORT!'")
								waitKey ()
								flags (1) = 0
								intro (1)
								'' JUMP!
								initWorld (4)	
							End If
						End If
						If k = 0 Then
							waitKey ()
							RestoreScreen ()
							plX = 21
						End If
					ElseIf hotspots (jdx + 4) = 3 Then
						' Otro pié.
					End If
					
					fsp21InitSprites ()
				End If
				j2dx = j2dx + 6
			Next ii
			
			'' Hotspots
		Wend
	Wend
	
	Pause 0
End Sub

Sub displayExtra (n As uByte)
	If n = 2 Then
		myPutChar4 (24, 10, 57, 23)
	End If
End Sub

Sub doStep ()
	plStep = plStep + 1: If plStep = 4 Then plStep = 0: End If
End Sub

Sub playerMove () 
	'' move Left / Right
	If In (57342) = 253 Or In (57342) = 189 Then
		plFacing = 48
		doStep ()
		' Can move?
		If plX  > 0 And gpIsHard (plX - 1, plY) And gpIsHard (plX - 1, plY + 1) Then
			plX = plX - 1
		End If
	End If
	If In (57342) = 254 Or In (57342) = 190 Then
		plFacing = 32
		doStep ()
		' Can move?
		If plX < 30 And gpIsHard (plX + 2, plY) And gpIsHard (plX + 2, plY + 1) Then
			plX = plX + 1
		End If
	End If
	'' move Up / Down
	If In (64510) = 254 Or In (64510) = 190 Then
		plFacing = 16
		doStep ()
		' Can move?
		If plY > 0 And gpIsHard (plX, plY - 1) And gpIsHard (plX + 1, plY - 1) Then
			plY = plY - 1
		End If
	End If
	If In (64766) = 254 Or In (64766) = 190 Then
		plFacing = 0
		doStep ()
		' Can move?
		If plY < 22 And gpIsHard (plX, plY + 2) And gpIsHard (plX + 1, plY + 2) Then
			plY = plY + 1
		End If
	End If
	
	'' Update Sprites
	updateSprites ()
	
	'' Move Sprites
	fsp21MoveSprite (3, plX, plY)
End Sub

Sub updateSprites ()
	If plStep = 0 Then
		fsp21SetGfxSprite (3, plFacing, plFacing + 1, plFacing + 2, plFacing + 3)
	ElseIf plStep = 1 Or plStep = 3 Then
		fsp21SetGfxSprite (3, plFacing + 4, plFacing + 5, plFacing + 6, plFacing + 7)
	Else
		fsp21SetGfxSprite (3, plFacing + 8, plFacing + 9, plFacing + 10, plFacing + 11)
	End If
End Sub

Sub myPutChar (x As uInteger, y As uInteger, c As uByte, tileIndex As uInteger)
	' Contains code by LCD modified to fit with my engine/data storage.

	Dim scr As uInteger
	Dim a As uInteger
	Dim adr As uInteger
	
	a = Peek (@linebuffer + y)
	scr = (a << 5) + x + 16384
	
	adr = @lhikojCharSet (0) + (tileIndex << 3) ' Peek (uInteger, 23606) + 256 + (tileIndex << 3)
	
	poke uByte scr, peek (adr)
	poke uByte scr + 256, peek (adr + 1)
	poke uByte scr + 512, peek (adr + 2)
	poke uByte scr + 768, peek (adr + 3)
	poke uByte scr + 1024, peek (adr + 4)
	poke uByte scr + 1280, peek (adr + 5)
	poke uByte scr + 1536, peek (adr + 6)
	poke uByte scr + 1792, peek (adr + 7)
	
	poke uByte 22528 + x + (y << 5), c
End Sub

Sub myPutChar4 (x As uInteger, y As uInteger, c As uByte, start As uInteger)
	myPutChar (x, y, c, start)
	myPutChar (x + 1, y, c, start + 1)
	myPutChar (x, y + 1, c, start + 2)
	myPutChar (x + 1, y + 1, c, start + 3)
End Sub

Sub scrollMsg (t1 As String, t2 As String, t3 As String)
	Dim y As uByte
	For y = 9 To 13
		Print At y, 4; "                        ";
	Next y
	' Preserve hardness map
	Asm
		ld hl, 62000
		ld de, 26048
		ld bc, 768
		ldir
	End Asm
	
	gpParseBinary (@scrolly)
	
	Print Inverse 1; At 10, 16 - Len (t1) / 2; t1; At 11, 16 - Len (t2) / 2; t2; At 12, 16 - Len (t3) / 2; t3;
	' Restore hardness map
	Asm
		ld hl, 26048
		ld de, 62000
		ld bc, 768
		ldir
	End Asm
End Sub

Sub waitKey ()
	While Inkey <> "": Wend
	While Inkey = "": Wend
End Sub

Sub SaveScreen ()
	fsp21BorraSprites ()					
	Asm
		ld hl, 18432
		ld de, 24000
		ld bc, 2048
		ldir
	End Asm
End Sub

Sub RestoreScreen ()
	Asm
		ld hl, 24000
		ld de, 18432
		ld bc, 2048
		ldir
	End Asm
End Sub

Sub fadeWhite ()
	Asm 
		ld   e, 3               ; 3 tercios 
		ld   hl, 22528          ; aquí empiezan los atributos 
		halt                    ; esperamos retrazo. 
	fade_out_bucle:
		ld   a, (hl )           ; nos traemos el atributo actual 
	 	
		ld   d, a               ; tomar atributo 
		and  7                  ; aislar la tinta 
		cp	 7
		jr   z, ink_done        ; si vale 7, no se incrementa 
		inc  a                  ; incrementamos tinta 
	ink_done:
		ld   b, a               ; en b tenemos ahora la tinta ya procesada. 
		
		ld   a, d               ; tomar atributo 
		and  56                 ; aislar el papel, sin modificar su posición en el byte 
		;jr   z, paper_done      ; si vale 0, no se decrementa 
		;sub  8                  ; decrementamos papel restando 8 
	paper_done:
		ld   c, a               ; en c tenemos ahora el papel ya procesado. 
		ld   a, d  
		and  192                ; nos quedamos con bits 6 y 7 (BRIGHT y FLASH) 
		or   c                  ; añadimos paper 
		or   b                  ; e ink, con lo que recompuesto el atributo 
		ld   (hl),a             ; lo escribimos, 
		inc  l                  ; e incrementamos el puntero. 
		jr   nz, fade_out_bucle ; continuamos hasta acabar el tercio (cuando L valga 0) 
		inc  h                  ; siguiente tercio 
		dec  e 
		jr   nz, fade_out_bucle ; repetir las 3 veces 
	End Asm
End Sub

Sub scrollRoutinesContainer ()
	Asm
	escrolleft:             
		;; calculamos hl
		
		ld   a, (scroll__line) ; nos traemos el numero de linea
		ld   l,a           ; lo guardamos para no tener que pillarlo otra vez
		add  a, $40        ;
		and  $58           ; nos quedamos con el tercio + $40
		ld   h, a
		
		ld   a, l          ; recuperamos el numero de linea
		and  7             ; linea dentro del tercio: linea MOD 8
		rrca
		rrca
		rrca               ; rotamos pa la derecha del byte
		ld   l, a   
		
		;;
		
		ld   a, 8          ; ocho lineas...
	buclebmp1:
		push af
		
		ld   d,h
		ld   e,l
		push bc
		push hl
		ld   a,(hl)
		inc  hl
		ld   bc, 31
		ldir
		dec  hl
		ld   (hl),a
		pop  hl
		pop  bc
		
		pop  af
		inc  h
		dec  a
		jr   nz, buclebmp1
		
		ret
	End Asm
scrollLine:
	Asm
	scroll__line:
		defb 0
	End Asm
End Sub

Sub introMessage (t1 As String, t2 As String)
	For i = 10 To 13
		Print at i, 4; inverse 1; "                        ";
	Next i
	Print Inverse 1;At 11, 16 - Len (t1) / 2; t1; At 12, 16 - Len (t2) / 2; t2;
End Sub

Sub clearScreen ()
	For i = 0 To 7: fadeWhite (): Next i: Ink 0: Paper 7: Cls
End Sub

Sub switchOnPlayerSprite ()
	fsp21ActivateSprite (3)
	fsp21MoveSprite (3, plX, plY)
	fsp21DuplicateCoordinatesSprite (3)
	fsp21InitSprites ()
End Sub

Sub intro (n As uByte)
	Dim terminado As uByte
	Dim state As uByte
	Dim ct As uByte
	Dim ct2 As uInteger
	Dim flipflop As uByte
	Dim maxState As uByte

	' Clear screen
	clearScreen ()
	
	' Draw backdrop
	displayScreen (99)
	SaveScreen ()
	
	myPutChar4 (12, 18, 56, 32)
	myPutChar (15, 19, 58, 27 + Int (Rnd * 5))
	
	' Unpack sky
	Print Paper 7; Ink 7; At 0,0; ,,,,,,,,,,,,,,,,
	aplibUnpack (@scrollablesky, 16384)
	
	' Fade in sky
	For i = 0 To 7
		Print Over 1; Paper 7; Ink 7-i; At 0,0; ,,,,,,,,,,,,,,,,
	Next i
	
	' Do inits
	If n = 0 Then
		state = 0
		plX = 30
		plY = 18
		plStep = 0
		plFacing = 48
		maxState = 8
		fsp21DeactivateSprite (3)
	ElseIf n > 0 Then
		state = 1
		plX = 17
		plY = 18
		plStep = 0
		plFacing = 48
		maxState = 7
		updateSprites ()
		switchOnPlayerSprite ()
	End If
	
	ct = 0
	ct2 = 0
	flipflop = 0
	
	' Do Intro
	While Not terminado
		If Inkey = " " Then terminado = 1: End If
	
		' General animation
		Asm
			halt
		End Asm
		
		flipflop = 1 - flipflop
		ct = ct + 1: If ct = 4 then ct = 0: Let ct2 = ct2 + 1: End If
		
		For i = 0 To 7	
			Poke @scrollLine, i
			Asm
				call escrolleft
			End Asm
		Next i
		
		myPutChar (15, 19, 58, 27 + Int (Rnd * 5))
		myPutChar (12, 18, 56, 32 + (flipflop << 2))
		myPutChar (13, 18, 56, 33 + (flipflop << 2))
		
		' Do actions
		If n = 0 Or n = 1 Then
			If ct = 0 Then
				If state = 0 Then
					If ct2 = 16 Then
						switchOnPlayerSprite ()
					ElseIf ct2 > 16 Then			
						plX = plX - 1
						doStep ()
						updateSprites ()
						fsp21MoveSprite (3, plX, plY)
						If plX = 17 Then state = 1: ct2 = 0: End If
					End If
				End If
				If state > 0 And state < maxState Then
					If ct2 = 4 Then 
						If n = 0 Then
							If state = 1 Then introMessage ("HELLO, ELDERLY WELLA.","I'M TERRIBLY TROUBLED"): End If
							If state = 2 Then introMessage ("I KEEP DREAMING THE SAME","NONSENSE EVERY NIGHT"): End If
							If state = 3 Then introMessage ("I FIND MYSELF IN STRANGE","PLACES UNKNOWN TO ME"): End If
							If state = 4 Then introMessage ("I NEED SOME ANSWERS.","IT'S DRIVING ME MAD."): End If
							If state = 5 Or state = 7 Then ct2 = 24: End If
							If state = 6 Then introMessage ("WELLA: LOOK INSIDE YOUR", "MIND TO FIND OUT..."): End If
						End If
						If n = 1 Then
							If state = 1 Then introMessage ("THEN EVERYTHING SUDDENLY", "DISAPPEARED..."): End If
							If state = 2 Then introMessage ("I FOUND MYSELF IN A","DIFFERENT PLACE."): End If
							If state = 3 Then introMessage ("BUT I FELT A STRANGE", "POWER IN MY HANDS!"): End If
							If state = 4 Or state = 6 Then ct2 = 24: End If
							If state = 5 Then introMessage ("I KNEW I HAD TO USE IT.","IT WAS VERY STRANGE."):End If
						End If
					End If
					If ct2 = 32 Then state = state + 1: ct2 = 0: RestoreScreen (): End If
					ct = 0
				End If
				If state = maxState Then
					terminado = 1
				End If
			End If
		End If
		
		fsp21UpdateSprites ()
	Wend
	
	clearScreen ()
End Sub
