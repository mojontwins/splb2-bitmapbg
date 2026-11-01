'' GfxParser for SUVLEIR 3.0
'' ZX Basic version
''
'' Uses Britlion's ZX Basic port of Alvin Albrecht's Patterned Floor Fill routine

'' So lets include it:
#include "SPPFill.bas"

'' Remove this if you don't want a hardness layer
Dim gpHardnessLayer (767) as uByte

'' Main function
Sub gpParseBinary (binAddress as uInteger)
	Dim endOfDrawing as uInteger
	Dim lsb, x, y, cx, cy, cmd, ex as uByte
	Dim dx, dy as Integer
	Dim msb as uInteger
	Dim hCt, hVal as uByte
	Dim i as uInteger
	
	' Fill endOfDrawing
	lsb = Peek (binAddress)
	msb = Peek (binAddress + 1)
	endOfDrawing = binAddress + lsb + (msb << 8) - 2
	binAddress = binAddress + 2
	
	' Plot and draw...	
	While (endOfDrawing > binAddress)
		cmd = Peek (binAddress)
		binAddress = binAddress + 1
		
		If cmd < 192 Then
			x = Peek (binAddress)
			binAddress = binAddress + 1
			y = 191 - cmd
			dx = x: dx = dx - cx ' ZX Basic doesn't like type mix
			dy = y: dy = dy - cy ' ZX Basic doesn't like type mix
			Draw dx, dy
			cx = x
			cy = y
		ElseIf cmd = 192 Then
			x = Peek (binAddress)
			y = 191 - Peek (binAddress + 1)
			binAddress = binAddress + 2
			Plot x, y
			cx = x
			cy = y
		ElseIf cmd = 193 Then
			x = Peek (binAddress)
			y = Peek (binAddress + 1)
			ex = Peek (binAddress + 2)
			'Print binAddress ; ": FILL ";x;",";y;",";ex;"      ":Pause 0
			binAddress = binAddress + 3
			Asm	
				push ix
			End Asm
			SPPFill (x, y, @gpPatterns + (ex << 3))
			Asm
				pop ix
			End Asm
		End If
	Wend
	
	' Un-RLE the hardness layer
	hCt = 0
	For i = 0 To 767
		If hCt = 0 Then 
			cmd = Peek (binAddress)
			binAddress = binAddress + 1
			hCt = cmd bAnd 63
			hVal = cmd >> 6
		End If
		gpHardnessLayer (i) = hVal

		hCt = hCt - 1
	Next i
End Sub

'' Read hardness layer
Function gpIsHard (x as uByte, y as uInteger) as uByte
	Return 1 - gpHardnessLayer ((y << 5) + x)
End Function

'' Now on to define some patterns:
Sub gpPatternContainer
	gpPatterns:
	Asm
		defb 11111111b
		defb 11111111b
		defb 11111111b
		defb 11111111b
		defb 11111111b
		defb 11111111b
		defb 11111111b
		defb 11111111b
		
		defb 01110111b
		defb 11111111b
		defb 11111111b
		defb 11111111b
		defb 01110111b
		defb 11111111b
		defb 11111111b
		defb 11111111b
	
		defb 01110111b
		defb 11111111b
		defb 11011101b
		defb 11111111b
		defb 01110111b
		defb 11111111b
		defb 11011101b
		defb 11111111b
	
		defb 01010101b
		defb 11111111b
		defb 01010101b
		defb 11111111b
		defb 01010101b
		defb 11111111b
		defb 01010101b
		defb 11111111b
			
		defb 10101010b
		defb 01010101b
		defb 10101010b
		defb 01010101b
		defb 10101010b
		defb 01010101b
		defb 10101010b
		defb 01010101b
		
		;defb 10101010b
		;defb 00000000b
		;defb 10101010b
		;defb 00000000b
		;defb 10101010b
		;defb 00000000b
		;defb 10101010b
		;defb 00000000b
		
		defb 11111111b
		defb 00010000b
		defb 00010000b
		defb 00010000b
		defb 11111111b
		defb 00000001b
		defb 00000001b
		defb 00000001b
		
		defb 10001000b
		defb 00000000b
		defb 00100010b
		defb 00000000b
		defb 10001000b
		defb 00000000b
		defb 00100010b
		defb 00000000b	
		
		defb 10001000b
		defb 00000000b
		defb 00000000b
		defb 00000000b
		defb 10001000b
		defb 00000000b
		defb 00000000b
		defb 00000000b	
	End Asm
End Sub
