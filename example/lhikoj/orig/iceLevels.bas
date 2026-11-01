'' ice Levels
'' code and data
'' for LHIKOJ

'' levels
'' 0 = empty, 1 = kill, 2 = brick, 3 = rock, 4 = exit

Sub iceLevelsContainer ()
	'' RLE Encoded levels (192 bytes into ~16 to 32? wow)
	iceLevel0: 
	Asm
		defb 00000000b, 00001010b, 01100001b, 00000101b
		defb 00000000b, 01000100b, 10000001b, 00100100b, 01000111b
		defb 01000101b, 00100100b, 10000001b, 01000110b
		defb 00000000b, 00000000b, 00010000b
	End Asm
	iceLevel1:
	Asm
		defb 00000000b, 00010010b, 01000001b, 00000010b
		defb 01000001b, 00000100b, 01000010b, 00000110b
		defb 01000001b, 00000010b, 01000001b, 00000011b
		defb 01000100b, 00000011b, 01100011b, 00100010b
		defb 01101010b, 10000001b
		defb 01100110b, 00100010b, 01100111b, 10000001b
		defb 00000010b, 01000001b, 00000110b, 01000100b
		defb 00000101b, 01000001b, 00000111b, 01000010b
		defb 00010100b, 00000000b
	End Asm
	iceLevel2:
	Asm
		defb 01010000b, 00000000b, 00001010b, 01000001b
		defb 00001000b, 01000001b, 00000011b, 01000001b
		defb 00000010b, 01000001b, 00000101b, 01100100b
		defb 00100011b, 01100111b, 10000001b, 01100001b
		defb 01100111b, 00100010b, 01100110b, 10000001b
		defb 00001100b, 01000001b, 00001111b, 01000001b
		defb 00000011b, 00000000b, 01010000b
	End Asm
	iceLevel3:
	Asm
		defb 00000000b, 01000001b, 00000111b, 01000001b
		defb 00000111b, 01000001b, 00000010b, 01000001b
		defb 00000001b, 01000001b, 00000010b, 01000001b
		defb 00000111b, 01000001b, 00000010b, 01000001b
		defb 00000001b, 01000001b, 00000010b, 01000001b
		defb 00000010b, 01000001b, 00000001b, 01000001b
		defb 00000010b, 01000001b, 00100010b, 01000001b
		defb 00100001b, 01000001b, 00100010b, 01000001b
		defb 00100001b, 01000001b, 00100011b, 01100001b
		defb 10000001b
		defb 01000001b, 00100001b, 01000001b, 00100001b
		defb 01000001b, 00100001b, 01000001b, 00100010b
		defb 01000001b, 01100100b, 10000001b, 01100001b
		defb 01000001b, 00000001b, 01000001b, 00000001b
		defb 01000001b, 00000001b, 01000001b, 00000010b
		defb 01000001b, 00000110b
		defb 01000001b, 00000001b, 01000001b, 00000001b
		defb 01000001b, 00000001b, 01000001b, 00000010b
		defb 01000001b, 00001010b
		defb 01000001b, 00000110b, 01000001b, 00000100b
		defb 00000000b
	End Asm
End Sub

Dim iceLevels (3) As uInteger

Dim iceLevelsTiles (9, 3) As uByte => { _
	{0, 0, 0, 0}, _
	{2, 3, 4, 5}, _
	{1, 1, 1, 1}, _
	{6, 7, 8, 9}, _
	{19, 20, 1, 1}, _
	{0, 0, 0, 0}, _
	{11, 12, 13, 14}, _
	{10, 10, 10, 10}, _
	{15, 16, 17, 18}, _
	{1, 1, 21, 22} _
}

Dim iceNPant as uByte

''

Sub drawIceTile (x as uByte, y as uByte, c as uByte, n as uByte)
	x = x + x
	y = y + y
	myPutChar (x, y, c, iceLevelsTiles (n, 0))
	myPutChar (x + 1, y, c, iceLevelsTiles (n, 1))
	myPutChar (x, y + 1, c, iceLevelsTiles (n, 2))
	myPutChar (x + 1, y + 1, c, iceLevelsTiles (n, 3))
End Sub

Function iceTileAt (x as uByte, y as uByte) As uByte
	Return Peek (62000 + (y << 4) + x) 'iceLevels (iceNPant, (y << 4) + x)
End Function

Sub iceLevelDraw (n as uByte)
	Dim iceAddr as uInteger
	Dim x, y, c as uByte
	Dim t, ct as uByte
	
	iceAddr = iceLevels (n)
	x = 0: y = 0: ct = 0
	
	While y < 12
		If ct = 0 Then
			t = Peek (iceAddr) >> 5
			ct = Peek (iceAddr) bAnd 31
			If ct = 0 Then ct = 32: End If
			iceAddr = iceAddr + 1
		Else
			ct = ct - 1
			If y < 6 Then 
				drawIceTile (x, y, 56, t)
			Else
				drawIceTile (x, y, 41, t + 5)
			End If
			Poke uByte (62000 + x + (y << 4)), t
			x = x + 1: If x = 16 Then x = 0: y = y + 1: End If
		End If
	Wend
	
End Sub

Sub initIcePlayer ()
	icePlX = 0: icePlX2 = icePlX
	icePlY = 0: icePlY2 = 22
	icePlSal = 0
	icePlSal2 = 0
	icePlNu = 0
	icePlNu2 = 0
	icePlFacing = 0
	icePlStep = 0
End Sub

Sub moveIcePlayer ()
	Dim xx as uByte
	Dim yy as uByte
	Dim xx2 as uByte
	Dim yy2 as uByte
	xx = icePlX >> 1
	yy = icePlY >> 1
	xx2 = icePlX2 >> 1
	yy2 = icePlY2 >> 1
 
	If icePlSal = 0 Then 
		If (((icePlY bAnd 1) = 1) Or (iceTileAt (xx, yy + 1) < 2 And (((icePlX bAnd 1) = 0) Or iceTileAt (xx + 1, yy + 1) < 2))) And icePlY < 10 Then
			icePlY = icePlY + 1
		End If
	End If
	If icePlSal2 = 0 Then
		If (((icePlY2 bAnd 1) = 1) Or (iceTileAt (xx2, yy2 - 1) < 2 And (((icePlX2 bAnd 1) = 0) Or iceTileAt (xx2 + 1, yy2 - 1) < 2))) And icePlY2 > 12 Then
			icePlY2 = icePlY2 - 1
		End If
	End If
	
	yy = icePlY >> 1
	yy2 = icePlY2 >> 1

	'' move Left / Right
	If In (57342) = 253 Or In (57342) = 189 Then
		icePlFacing = 8
		icePlStep = 4 - icePlStep
		If (((icePlX bAnd 1) = 1) Or (iceTileAt (xx - 1, yy) < 2 And (((icePlY bAnd 1) = 0) Or iceTileAt (xx - 1, yy + 1) < 2))) And icePlX > 0 Then
			icePlX = icePlX - 1
		End If
		If (((icePlX2 bAnd 1) = 1) Or (iceTileAt (xx2 - 1, yy2) < 2 And (((icePlY2 bAnd 1) = 0) Or iceTileAt (xx2 - 1, yy2 + 1) < 2))) And icePlX2 > 0 Then
			icePlX2 = icePlX2 - 1
		End If
	End If
	
	If In (57342) = 254 Or In (57342) = 190 Then
		icePlFacing = 0
		icePlStep = 4 - icePlStep
		If (((icePlX bAnd 1) = 1) Or (iceTileAt (xx + 1, yy) < 2 And (((icePlY bAnd 1) = 0) Or iceTileAt (xx + 1, yy + 1) < 2))) And icePlX < 30 Then
			icePlX = icePlX + 1
		End If
		If (((icePlX2 bAnd 1) = 1) Or (iceTileAt (xx2 + 1, yy2) < 2 And (((icePlY2 bAnd 1) = 0) Or iceTileAt (xx2 + 1, yy2 + 1) < 2))) And icePlX2 < 30 Then
			icePlX2 = icePlX2 + 1
		End If
	End If
	
	'' Jump
	
	xx = icePlX >> 1
	xx2 = icePlX2 >> 1
	
	If icePlSal = 0 Then
		If In (32766) = 254 Or In (32766) = 190 Then
			If icePlY = 10 Or ((icePlY bAnd 1) = 0 And (iceTileAt (xx, yy + 1) > 1 Or (((icePlX bAnd 1) = 1) And iceTileAt (xx + 1, yy + 1) > 1))) Then
				icePlSal = 1
				icePlNu = 0
			End If
		End If
	Else
		If (((icePlY bAnd 1) = 1) Or (iceTileAt (xx, yy - 1) < 2 And ((icePlX bAnd 1) = 0 Or iceTileAt (xx + 1, yy - 1) < 2))) And icePlY > 0 Then
			icePlY = icePlY - 1
		End If
		icePlNu = icePlNu + 1
		If icePlNu = 6 Then icePlSal = 0: End If
	End If
	If icePlSal2 = 0 Then
		If In (32766) = 254 Or In (32766) = 190 Then
			If icePlY = 12 Or ((icePlY2 bAnd 1) = 0 And (iceTileAt (xx2, yy2 - 1) > 1 Or (((icePlX2 bAnd 1) = 1) And iceTileAt (xx2 + 1, yy2 - 1) > 1))) Then
				icePlSal2 = 1
				icePlNu2 = 0
			End If
		End If
	Else
		If (((icePlY2 bAnd 1) = 1) Or (iceTileAt (xx2, yy2 + 1) < 2 And ((icePlX2 bAnd 1) = 0 Or iceTileAt (xx2 + 1, yy2 + 1) < 2))) And icePlY2 < 22 Then
			icePlY2 = icePlY2 + 1
		End If
		icePlNu2 = icePlNu2 + 1
		If icePlNu2 = 6 Then icePlSal2 = 0: End If
	End If
	
	'' Update Sprites
	
	fsp21SetGfxSprite (0, 64 + icePlFacing + icePlStep, 65 + icePlFacing + icePlStep, 66 + icePlFacing + icePlStep, 67 + icePlFacing + icePlStep)
	fsp21SetGfxSprite (1, 80 + icePlFacing + icePlStep, 81 + icePlFacing + icePlStep, 82 + icePlFacing + icePlStep, 83 + icePlFacing + icePlStep)
	
	fsp21MoveSprite (0, icePlX, icePlY)
	fsp21MoveSprite (1, icePlX2, icePlY2)
End Sub

Sub iceGame (whichOne as uByte)
	Dim xx as uByte
	Dim yy as uByte
	Dim xx2 as uByte
	Dim yy2 as uByte
	Dim repeat as uByte
	Dim terminado as uByte

	iceNPant = whichOne
	Paper 0: Cls: Paper 7
	
	repeat = 1
	While repeat = 1
		iceLevelDraw (iceNPant)
		initIcePlayer ()
		
		fsp21SetGfxSprite (0, 64, 65, 66, 67)
		fsp21SetGfxSprite (1, 80, 81, 82, 83)
		fsp21MoveSprite (0, icePlX, icePlY)
		fsp21MoveSprite (1, icePlX2, icePlY2)
		fsp21DuplicateCoordinatesSprite (0)
		fsp21DuplicateCoordinatesSprite (1)
		fsp21ActivateSprite (0)
		fsp21ActivateSprite (1)
		fsp21DeactivateSprite (3)
		
		fsp21InitSprites ()
	
		terminado = 0
		While terminado = 0
			moveIcePlayer ()
			asm
				halt
				halt
			end asm
			fsp21UpdateSprites ()
			
			xx = icePlX >> 1
			yy = icePlY >> 1
			xx2 = icePlX2 >> 1
			yy2 = icePlY2 >> 1
			
			' Detect exit condition
			If (icePlY bAnd 1) = 0 And (icePlX bAnd 1) = 0 And iceTileAt (xx, yy + 1) = 4 And _
			   (icePlY2 bAnd 1) = 0 And (icePlX2 bAnd 1) = 0 And iceTileAt (xx2, yy2 - 1) = 4 Then
				If obonabilobos (iceNPant) = 0 Then
					scrollMsg ("CONGRATULATIONS!", " ", "YOU GOT AN OBONABILOBO")
					obonabilobos (iceNPant) = 1
					plObonabilobos = plObonabilobos + 1
				Else
					scrollMsg ("CONGRATULATIONS!", "BUT THE OBONABILOBO HERE", "WAS ALREADY TAKEN...")
				End If
				waitKey ()
				terminado = 1
				repeat = 2
			End If
			
			' Detect kill condition
			If ((icePlY bAnd 1) = 0 And (iceTileAt (xx, yy) = 1 Or ((icePlX bAnd 1) = 1 And iceTileAt (xx + 1, yy) = 1))) _
			Or ((icePlY2 bAnd 1) = 0 And (iceTileAt (xx2, yy2) = 1 Or ((icePlX2 bAnd 1) = 1 And iceTileAt (xx2 + 1, yy2) = 1))) Then
				scrollMsg ("OUCH!", " ", "TRY AGAIN Y/N?")
				repeat = 0
				While repeat = 0
					If Inkey = "y" Or Inkey = "Y" then repeat = 1: End If
					If Inkey = "n" Or Inkey = "N" then repeat = 2: End If
				Wend
				terminado = 1
			End If
		Wend
	Wend
	fsp21DeactivateSprite (0)
	fsp21DeactivateSprite (1)
	Paper 7: Cls
End Sub
