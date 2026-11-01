''
'' Eleuterio by The Mojon Twins
'' A game for the Radascompo 2010
'' big hamma platforming action!
''
'' Copyleft 2010 by the Mojon Twins
'' http://www.mojontwins.com
''
'' Compiles using Boriel's ZX Basic v. 1.2.6 r 1564
'' http://www.boriel.com/software/the-zx-basic-compiler/
''

#include once "fsp2.1.bas"					
#include once "spritegfxdata.bas"			
#include once "tilegfxdata.bas"				

'' Global array definitions (TODO: move to other BAS file when engine is complete)
'' 50 * no_of_levels - 1

'' For platform life gauges, 12 is maximum (complete), 0 is minimum.

Dim hammeringLevels (0, 42) As uByte => { _
	{ _
		64, _									
		12, 8, 8, 4, 12, 8, _					
		_
		33, _									
		0, 4, 12, 12, 12, 12, _					
		_
		6, _									
		12, 12, 8, 12, 12, 12, _				
		_
		80, _									
		12, 12, 12, 12, 12, 12, _		
		_		
		0, _									
		32, _									
		8, _									
		1, _									
		_
		1, _									
		24, _									
		8, _									
		0, _									
		_
		2, _									
		32, _									
		12, _									
		1, _									
		_
		0, _                                   
        2, _                                   
        3 _     							
	}_
}

Dim hammeringBigTiles (1, 15) As uByte		' Big tiles to draw the background
Dim curScreenBuf (4, 12) As uByte			' Intermediate screen buffer with info

' Player data

Dim plFacing As uByte						' player facing 0 = left, 8 = right
Dim plX As uByte							' player X
Dim plY As uByte							' player Y
Dim plState As uByte						' player FSM state
Dim plStateData As uByte					' player extra data related to FSM state
Dim plLife As uByte							' player life gauge
Dim plHammerStrength As uByte				' player's hammer strength
Dim plPant As uByte

' Enemy data

Dim enFacing (2) As uByte					' enemies facing 0 = left, 8 = right
Dim enX (2) As uByte						' enemies X
Dim enY (2) As uByte						' enemies Y
Dim enState (2) As uByte					' enemies FSM state
Dim enStateData (2) As uByte				' enemies extra data related to FSM state
Dim enLife (2) As uByte						' enemies life gauge
Dim enLastLife (2) As uByte					' enemies last life gauge
Dim enT (2) As uByte						' enemies AI type
Dim enC (2) As uByte						' enemies colour
Dim enSpd (2) As uByte						' enemies speed
Dim enFix (2) As uByte						' enemies turns to fix a hole
Dim enCoscao (2) As uByte					' enemies aggresiveness, 1=chases you.
Dim enOldState (2) As uByte					' enemies previous state.
Dim enHalfLife As uByte						' enemies half counter

' Line buffer (for LCD's putChar routine and others)

linebuffer:
asm
  defb 0,1,2,3,4,5,6,7,64,65,66,67,68,69,70,71,128,129,130,131,132,133,134,135
end asm

'' Main program

Border 0: Paper 0: Ink 0: Cls

fsp21SetGfxAddress (@spriteGfxData (0))

drawHammerinLevel (0)
initEnemiesHammerinLevel (0)
initPlayerHammerinLevel ()
fsp21InitSprites ()
plHammerStrength = 2	'Pa probar
'Ink 7: Poke uInteger 23606, 60*256

While Inkey$ <> "h"
	moveHammerinLevel ()
	moveEnemiesHammerinLevel ()
	
	asm
		halt
		halt
	end asm
	
	fsp21UpdateSprites ()
Wend

' Return to BASIC in a friendly way...
Ink 7: Poke uInteger 23606, 60*256: Pause 0




End

'' Subs and functions

Sub initEnemiesHammerinLevel (nPant As uByte)
	' This Sub reads the level data pointed by nPant and initializes the three
	' enemies.
	
	Dim i As uByte
	Dim c As uByte
	Dim b As uByte
	
	enHalfLife = 0	
	
	enX (0) = 3								' Enemy 0 starts to the left
	enX (1) = 15							' Enemy 1 starts in the middle
	enX (2) = 27							' Enemy 2 starts to the right
	
	For i = 0 To 2
		enY (i) = 5							' Top platform
		enState (i) = 0						' state = idling
		enFacing (i) = 0					' facing left
		enCoscao (i) = 0					' not aware
		enOldState (i) = 0					' Old state.
		
		' Data retrieved from hammeringLevels (nPant):
		
		enT (i) = hammeringLevels (nPant, 28 + (i << 2))
		enFix (i) = hammeringLevels (nPant, 29 + (i << 2))
		enLife (i) = hammeringLevels (nPant, 30 + (i << 2))
		enSpd (i) = hammeringLevels (nPant, 31 + (i << 2))
		enLastLife (i) = enLife (i)
		
		' Sprite colour depending on type
		
		If enT (i) = 0 Then
			c = 6
		ElseIf enT (i) = 1 Then
			c = 4
		ElseIf enT (i) = 2 Then
			c = 5
		ElseIf enT (i) = 3 Then
			c = 7
		End If
		enC (i) = c
		
		' Sprite gfx blocks depending on type
		
		b = (enT (i) + 1) << 5
		
		' fsp2.1 stuff
		
		fsp21SetGfxSprite (i, b, b + 1, b + 2, b + 3)
		fsp21ColourSprite (i, 64 + c, 64 + c, c, c)
		fsp21MoveSprite (i, enX (i), enY (i))
		fsp21DuplicateCoordinatesSprite (i)
		fsp21ActivateSprite (i)
	Next i
End Sub

Sub initPlayerHammerinLevel ()
	plX = 3
	plY = 21
	plState = 0
	
	' fsp 2.1 stuff
	
	fsp21SetGfxSprite (3, 8, 9, 10, 11)
	fsp21ColourSprite (3, 71, 71, 71, 71)
	fsp21MoveSprite (3, plX, plY)
	fsp21DuplicateCoordinatesSprite (3)
	fsp21ActivateSprite (3)
End Sub

Sub enemySelectRandomEnState (i As uByte, enBufX As uByte, enBufY As uByte)
	Dim j As uByte

	' This sub selects a random direction for an enemy.
	While enState (i) = 0		' Keep deciding until state is correct
		j = Int (Rnd * 6)
		
		If j = 0 And enX (i) > 3 Then
			enState (i) = 1
			enStateData (i) = enX (i) - 2
			enFacing (i) = 0
		ElseIf j = 1 And enX (i) < 27 Then
			enState (i) = 2
			enStateData (i) = enX (i) + 2
			enFacing (i) = 8
		ElseIf j = 2 Or j = 3 Then
			If enBufY > 0 Then
				If curScreenBuf (enBufY - 1, enBufX) = 128 Then
					enState (i) = 3
					enStateData (i) = enY (i) - 4
				End If
			End If
		Else
			If curScreenBuf (enBufY, enBufX) = 128 And enBufY < 3 Then
				enState (i) = 4
				enStateData (i) = enY (i) + 4
			End If
		End If
	Wend
End Sub

Sub moveEnemiesHammerinLevel ()
	' This Sub moves enemies in hammerin levels. This game works with four
	' different AIs. I'll try to code this function in a readable way... But
	' I can't promise anything :-P
	
	' FSMs again, of course.
	
	' AI type usually is used to decide when the enemy enters in aggresive
	' mode and starts chasing you. If not in aggresive mode, enemy moves
	' randomly.
	
	' All decisions happen on or under stairs. When enemies reach a wall, they
	' just turn around. AI type 3 is more persistent: on aggresive mode, it
	' decides a new direction anywhere.
	
	Dim enBufX As uByte
	Dim enBufY As uByte
	
	Dim changeDirection As uByte
	Dim tileIndex As uByte
	
	Dim i As uByte
	Dim j As uByte
	Dim b As uByte
	
	enHalfLife = 1 - enHalfLife
	
	For i = 0 To 2
	
		'' Precalculations
	
		' Coordinates in our data buffer
	
		enBufY = (enY (i) - 5) >> 2
		enBufX = (enX (i) - 3) >> 1
		
		'' Kill the enemy if it reaches the bottom floor
		
		If enBufY > 3 And enState (i) < 7 Then 		
			enState (i) = 7							' Change state to dying
			enStateData (i) = 0						' Frame count
		End If
		
		' Sprite gfx blocks depending on type
		
		b = (enT (i) + 1) << 5
		
		'' General checks.
		
		If enLife (i) = 0 Then
			enState (i) = 5							' Change state to falling
			enStateData (i) = enY (i) + 2			' Destination
			enLife (i) = enLastLife (i) / 2			
			If enLife (i) = 0 Then enLife (i) = 1: End If
			enLastLife (i) = enLife (i)
		End If
		
		If enState (i) = 0 Then						' idling.
			If enBufY < 4 Then						' Check if enemy falls to a broken tile to fix it
				If curScreenBuf (enBufY, enBufX) = 0 Then		
					enState (i) = 6					' Change state to fixing
					enStateData (i) = 0				' Frame count
				End If
			End If
		End If
	
		'' Enemy movement depending on type of AI
		
		If enState (i) = 0 Then						' if still idling, we can do whatever. 
		
			' First we decide whether the enemy is in aggresive mode or not.
			' This depends on enT (i)
			
			If enT (i) = 0 Then						' AI type 0: The blind. 
				enCoscao (i) = 0					' 	Hill never notice.
			ElseIf enT (i) = 1 Then					' AI type 1: Stupid. 
				If enY (i) = plY Then				' 	notices you if on the same floor.
					enCoscao (i) = 1
				Else
					enCoscao (i) = 0
				End If
			ElseIf enT (i) = 2 Then					' AI type 2: Smelly.
				If enY (i) >= plY - 4 And _
					enY (i) <= plY + 4 Then			'	notices you if you are beneath or above
					enCoscao (i) = 1
				Else
					enCoscao (i) = 0
				End If
			ElseIf enT (i) = 3 Then					' AI type 3: Hound. 
				enCoscao (i) = 1					' Always aggressive.
			End If
			
			' Now we try to select a new state for our enemy
		
			changeDirection = 0
		
			' We only change the direction on stairs, boundaries, and the first time
			
			If enOldState (i) = 0 Then
				changeDirection = 1
			End If
			If curScreenBuf (enBufY, enBufX) = 128 Then
				changeDirection = 1
			End If
			If enBufY > 0 Then
				If curScreenBuf (enBufY - 1, enBufX) = 128 Then
					changeDirection = 1
				End If
			End If
			If enT (i) = 3 And enCoscao (i) = 1 Then
				changeDirection = 1
			End If
			If enX (i) = 3 Or enX (i) = 27 Then
				changeDirection = 1
			End If
			
			' If changeDirection has become TRUE, we decide a new direction 
			' depending on the enemy's aggresiveness.
			
			If changeDirection Then					' Decide a new direction
				If enCoscao (i) = 0 Then			' Not aggresive yet. Random, then
					
					' Anywhere: decide randomly
					
					enemySelectRandomEnState (i, enBufX, enBufY)
				Else								
					' In aggresive state, the enemy chases you. I've implemented 
					' greedy AI, doesn't search for path. The screen is simple enough
					' for this to work OK
					
					' First we check if we have to climb up/down (if we can)
					' We give priority to this.
					
					' UP
					If enBufY > 0 Then
						If curScreenBuf (enBufY - 1, enBufX) = 128 Then
							If plY < enY (i) Then
								enState (i) = 3
								enStateData (i) = enY (i) - 4
							End If
						End If
					End If
					
					If enBufY < 3 Then
						If curScreenBuf (enBufY, enBufX) = 128 Then
							If plY > enY (i) Then
								enState (i) = 4
								enStateData (i) = enY (i) + 4
							End If
						End If
					End If
					
					If enState (i) = 0 Then
						' If we haven't selected UP/DOWN yet, ('cause it was impossible
						' or 'cause Eleuterio is on the same floor) we select left/right
						If plX > enX (i) Then
							enState (i) = 2
							enStateData (i) = enX (i) + 2
						ElseIf plX < enX (i) Then
							enState (i) = 1
							enStateData (i) = enX (i) - 2
						End If
					End If
					
					' If we still haven't selected UP/DOWN YET we just move.
					If enState (i) = 0 Then
						enemySelectRandomEnState (i, enBufX, enBufY)
					End If
					
				End If
				
				enOldState (i) = enState (i)		' Refresh old direction		
			Else									' Keep old direction
				If enOldState (i) = 1 Then
					enState (i) = 1
					enStateData (i) = enX (i) - 2
				ElseIf enOldState (i) = 2 Then
					enState (i) = 2
					enStateData (i) = enX (i) + 2
				End If
				
			End If
		End If
		
		'' enemy actions
		
		If enHalfLife <= enSpd (i) Then					' If enSpd (i) = 1, enemy is slow
			If enState (i) = 1 Then
				' walking left
				enX (i) = enX (i) - 1
				
				' Select frame
				If enX (i) = (enX (i) >> 1) << 1 Then
					fsp21SetGfxSprite (i, b + 4, b + 5, b + 6, b + 7)
				Else
					fsp21SetGfxSprite (i, b, b + 1, b + 2, b + 3)
				End If
				
				' Check destination
				If enX (i) = enStateData (i) Then 
					enState (i) = 0						' Idling again
				End If
			ElseIf enState (i) = 2 Then
				' walking right
				enX (i) = enX (i) + 1
				
				' Select frame
				If enX (i) = (enX (i) >> 1) << 1 Then
					fsp21SetGfxSprite (i, b + 12, b + 13, b + 14, b + 15)
				Else
					fsp21SetGfxSprite (i, b + 8, b + 9, b + 10, b + 11)
				End If
				
				' Check destination
				If enX (i) = enStateData (i) Then 
					enState (i) = 0						' Idling again
				End If
			ElseIf enState (i) = 3 Then
				' climbing up
				enY (i) = enY (i) - 1
				
				' Select frame
				If enY (i) = (enY (i) >> 1) << 1 Then
					fsp21SetGfxSprite (i, b + 16, b + 17, b + 18, b + 19)
				Else
					fsp21SetGfxSprite (i, b + 20, b + 21, b + 22, b + 23)
				End If
				
				' Check destination
				If enY (i) = enStateData (i) Then
					enState (i) = 0						'Idling again
				End If
			ElseIf enState (i) = 4 Then
				' climbing down
				enY (i) = enY (i) + 1
				
				' Select frame
				If enY (i) = (enY (i) >> 1) << 1 Then
					fsp21SetGfxSprite (i, b + 16, b + 17, b + 18, b + 19)
				Else
					fsp21SetGfxSprite (i, b + 20, b + 21, b + 22, b + 23)
				End If
				
				' Check destination
				If enY (i) = enStateData (i) Then
					enState (i) = 0						'Idling again
				End If
			End If
		End If
		
		If enState (i) = 5 Then
			' fall down
			enY (i) = enY (i) + 1
			
			' Select Frame
			fsp21SetGfxSprite (i, b + enFacing (i), b + 1 + enFacing (i), _
				b + 2 + enFacing (i), b + 3 + enFacing (i))
			
			' Check destination
			If enY (i) = enStateData (i) Then
				enState (i) = 0						'Idling again
			End If
		ElseIf enState (i) = 6 Then
			' Fixing a hole
			' The first two frames are used to make the sprite descend to the hole.
			' The first frame we mark the hole as "someone fixing" as well
			If enStateData (i) = 0 Then
				curScreenBuf (enBufY, enBufX) = 64	' Someone fixing!
			End If
			
			If enStateData (i) < 2 Then
				' Descending to a hole
				enY (i) = enY (i) + 1
			
				' Select frame
				If enY (i) = (enY (i) >> 1) << 1 Then
					fsp21SetGfxSprite (i, b + 16, b + 17, b + 18, b + 19)
				Else
					fsp21SetGfxSprite (i, b + 20, b + 21, b + 22, b + 23)
				End If
			ElseIf enStateData (i) < enFix (i) + 2 Then
				' Fixing the hole
				
				' Select frame
				If enStateData (i) = (enStateData (i) >> 1) << 1 Then
					fsp21SetGfxSprite (i, b + 24, b + 25, b + 26, b + 27)
				Else
					fsp21SetGfxSprite (i, b + 28, b + 29, b + 30, b + 31)
				End If
			ElseIf enStateData (i) < enFix (i) + 4 Then
				' Climbing out the hole
				enY (i) = enY (i) - 1
			
				' Select frame
				If enY (i) = (enY (i) >> 1) << 1 Then
					fsp21SetGfxSprite (i, b + 16, b + 17, b + 18, b + 19)
				Else
					fsp21SetGfxSprite (i, b + 20, b + 21, b + 22, b + 23)
				End If
			Else
				' Restore platform
				curScreenBuf (enBufY, enBufX) = 12
				tileIndex = hammeringLevels (plPant, 40) * 6
				tilePutHalfBlock (enX (i), enY (i) + 2, tileIndex)
				tileIndex = 36 + (hammeringLevels (plPant, 41) << 3)
				tilePutHalfBlockReverse (enX (i), enY (i) + 3, tileIndex + 4)
				If enBufX < 6 Then
					If curScreenBuf (enBufY, enBufX + 1) <> 128 Then
						tilePutHalfBlockReverse (enX (i) + 2, enY (i) + 3, tileIndex + 4)
					End If
				End If
				
			End If
			
			' Update / check framecount
			enStateData (i) = enStateData (i) + 1
			If enStateData (i) = enFix (i) + 5 Then 
				enState (i) = 0
			End If
		ElseIf enState (i) = 7 Then
			' Dying. First 8 frames explosion.
			' Last frame the tomb.
			If enStateData (i) < 8 Then
				' Select frame
				If enStateData (i) = (enStateData (i) >> 1) << 1 Then
					fsp21SetGfxSprite (i, 224, 225, 226, 227)
				Else
					fsp21SetGfxSprite (i, 228, 229, 230, 231)
				End If
			Else
				fsp21SetGfxSprite (i, 232, 233, 234, 235)
			End If
			
			' Update / check framecount
			enStateData (i) = enStateData (i) + 1
			If enStateData (i) = 9 Then
				enState (i) = 8						' Disable enemy.
				enCoscao (i) = 0					' Uncoscate enemy.
													' (Spanish readers will have a laugh)
			End If
		End If
		
		'' fsp21 stuff
		
		fsp21MoveSprite (i, enX (i), enY (i))
		If enCoscao (i) = 1 And enHalfLife = 1 Then
			fsp21ColourSprite (i, 67, 67, 3, 3)
		Else
			fsp21ColourSprite (i, 64 + enC (i), 64 + enC (i), enC (i), enC (i))
		End If
	Next i
End Sub

Sub moveHammerinLevel () 
	' This Sub moves the player in hammerin levels. Everything is inside a nice
	' FSM to control what the player is doing at every moment. Movement is semi-
	' automatic for handheld-like gameplay and fast action (also to restrict
	' where the player can hit with his hamma.
	
	Dim plBufX As uByte
	Dim plBufY As uByte
	Dim tileIndex As uByte
	Dim i As uByte
	Dim enemyFound As uByte
	
	' First we calculate the actual position in the buffer
	
	plBufY = (plY - 5) >> 2
	plBufX = (plX - 3) >> 1

	If plState = 0 Then
		' idling.
		' Check if Eleuterio falls to the lower floor
		If plBufY < 4 Then 
			If curScreenBuf (plBufY, plBufX) = 0 Then
				plState = 5							' Change state to falling
				plStateData = plY + 4				' Destination
			End If
		End If
	End If
	
	If plState = 0 Then
		' still idling.
		' When idling, we can do whatever. Let's check the keyboard.
		If In (57342) = 253 Or In (57342) = 189 Then
			plFacing = 0
			If plX > 3 Then
				plState = 1							' Change state to walking left
				plStateData = plX - 2				' Destination
			End If
		ElseIf In (57342) = 254 Or In (57342) = 190 Then
			plFacing = 8
			If plX < 27 Then
				plState = 2							' Change state to walking right
				plStateData = plX + 2				' Destination		
			End If
		End If
		
		If In (64510) = 254 Or In (64510) = 190 Then
			If plBufY > 0 Then
				If curScreenBuf (plBufY - 1, plBufX) = 128 Then
					plState = 3						' Change state to climbing stairs up
					plStateData = plY - 4			' Destination
				End If
			End If
		ElseIf In (65022) = 254 Or In (65022) = 190 Then
			If plBufY < 4 Then
				If curScreenBuf (plBufY, plBufX) = 128 Then
					plState = 4						' Change state to climbing stairs down
					plStateData = plY + 4			' Destination
				End If
			End If
		End If
		
		If In (32766) = 254 Or In (32766) = 190 Then
			plState = 6								' Change state to hitting
			plStateData = 0							' Hitting frame (0-1)
		End If
		
		If plState = 0 Then
			fsp21SetGfxSprite (3, 0 + plFacing, 1 + plFacing, 2 + plFacing, 3 + plFacing)
		End If
	End If
	
	If plState = 1 Then
		' walking left
		plX = plX - 1
		
		' Select frame
		If plX = (plX >> 1) << 1 Then
			fsp21SetGfxSprite (3, 4, 5, 6, 7)
		Else
			fsp21SetGfxSprite (3, 0, 1, 2, 3)
		End If
		
		' Check destination
		If plX = plStateData Then 
			plState = 0		' Idling again
		End If
	ElseIf plState = 2 Then
		' walking right
		plX = plX + 1
		
		' Select frame
		If plX = (plX >> 1) << 1 Then
			fsp21SetGfxSprite (3, 12, 13, 14, 15)
		Else
			fsp21SetGfxSprite (3, 8, 9, 10, 11)
		End If
		
		' Check destination
		
		If plX = plStateData Then 
			plState = 0		' Idling again
		End If
	ElseIf plState = 3 Then
		' climbing up
		plY = plY - 1
		
		' Select frame
		If plY = (plY >> 1) << 1 Then
			fsp21SetGfxSprite (3, 16, 17, 18, 19)
			Beep .005, 0
		Else
			fsp21SetGfxSprite (3, 20, 21, 22, 23)
			Beep .005, 2
		End If
		
		' Check destination
		If plY = plStateData Then
			plState = 0		'Idling again
		End If
		
		' TODO: Change climbing mid-stairs
	ElseIf plState = 4 Then
		' climbing down
		plY = plY + 1
		
		' Select frame
		If plY = (plY >> 1) << 1 Then
			fsp21SetGfxSprite (3, 16, 17, 18, 19)
			Beep .005, 0
		Else
			fsp21SetGfxSprite (3, 20, 21, 22, 23)
			Beep .005, 10
		End If
		
		' Check destination
		If plY = plStateData Then
			plState = 0		'Idling again
		End If
		
		' TODO: Change climbing mid-stairs
	ElseIf plState = 5 Then
		' falling
		plY = plY + 1
		Beep .005, 30 - plY
		
		' Select frame
		fsp21SetGfxSprite (3, 4 + plFacing, 5 + plFacing, 6 + plFacing, 7 + plFacing)
		
		' Check destination
		If plY = plStateData Then
			plState = 0		'Idling again
		End If
	ElseIf plState = 6 Then
		' hitting
		
		' Select Frame
		If plStateData = 0 Or plStateData = 2 Then
			fsp21SetGfxSprite (3, 24, 25, 26, 27)
		Else
			fsp21SetGfxSprite (3, 28, 29, 30, 31)
		End If
		
		' Ending condition
		plStateData = plStateData + 1
		If plStateData = 3 Then
			' When the counter reaches 3, the animation is done. Now we
			' check if we have to damage something. Mainly, if we are
			' on floors 0, 1, 2, or 3 and there's a piece of platform
			' beneath Eleuterio. Stairs cannot be broken. They are made
			' of Adamantium.
			
			Beep .005, 0
			
			If plBufY < 4 Then 
				If curScreenBuf (plBufY, plBufX) < 64 Then
				
					' A breakable platform. Let's wreak some havok, guys!
				
					If curScreenBuf (plBufY, plBufX) > plHammerStrength Then
						curScreenBuf (plBufY, plBufX) = _
							curScreenBuf (plBufY, plBufX) - plHammerStrength
					Else
						curScreenBuf (plBufY, plBufX) = 0
					End If
					
					' Now we update the screen. Now, as always, the tricky,
					' ugly, undocumented, patched, crappy coding. But it works.
					' And it's fast. Sorry, this is 8 bits.
					
					If curScreenBuf (plBufY, plBufX) > 3 Then
						tileIndex = hammeringLevels (plPant, 40) * 6
						tilePutHalfBlock (plX, plY + 2, _
							tileIndex + 2 * (3 - (curScreenBuf (plBufY, plBufX) >> 2)))
					Else
						curScreenBuf (plBufY, plBufX) = 0
						tileIndex = (hammeringLevels (plPant, 41) << 3) + 36
						If plBufX > 0 And plBufY > 0 Then
							If curScreenBuf (plBufY - 1, plBufX - 1) = 128 Then
								myPutChar (plX, plY + 2, tileIndex + 5)
								myPutChar (plX + 1, plY + 2, tileIndex)
							Else	
								tilePutHalfBlockReverse (plX, plY + 2, tileIndex)
							End If
						Else
							tilePutHalfBlockReverse (plX, plY + 2, tileIndex)
						End If
						If plBufX < 12 Then
							If curScreenBuf (plBufY, plBufX + 1) = 128 Then
								myPutChar (plX + 1, plY + 3, tileIndex + 2)
								myPutChar (plX + 2, plY + 3, 92)
							Else
								tilePutHalfBlock (plX + 1, plY + 3, tileIndex + 2)
							End If
						Else
							tilePutHalfBlock (plX + 1, plY + 3, tileIndex + 2)
						End If
					End If
					
					Beep .005, -10
				ElseIf curScreenBuf (plBufY, plBufX) = 64 Then
				
					' Oopsies! An enemy! Let's beat the crap out of it!
					' First we find which one is the victim. Mwa ha ha
					
					enemyFound = 3
					For i = 0 To 2
						If enX (i) = plX And enY (i) = plY + 2 And enState (i) = 6 Then
							enemyFound = i
							Exit For
						End If
					Next i
					
					' Now... 
					
					If enemyFound < 3 Then
						If enLife (enemyFound) > plHammerStrength Then
							enLife (enemyFound) = enLife (enemyFound) - plHammerStrength
							If enStateData (enemyFound) > 0 Then
								enStateData (enemyFound) = enStateData (enemyFound) - 1
							End If
						Else
							enLife (enemyFound) = 0
							
							' Restore hole.
							curScreenBuf (plBufY, plBufX) = 0
						End If
					End If
					
					Beep .005, 10
				Else
					Beep .005, 50
				End If
			End If		
		
			plState = 0		' Idling again
		End If
	End If
	
	fsp21MoveSprite (3, plX, plY)					' Moves the sprite to new location.
End Sub

Sub drawHammerinLevel (nPant As uByte) 
	' This sub draws hammerin' level #nPant to the screen
	' according to the data stored in hammeringLevels.
	
	' This sub was a real PAIN IN THE ASS (PITA for the easily
	' impressed by certain combinations of letters), so don't
	' ask me how it works 'cause probably I've forgoten already.
	' The PITAness was due to the auto-generated shadows for
	' platforms and stairs and my wish not to have to substitute
	' any character on screen (which looks ugly, cheesy, and
	' noobish).

	Dim x As uByte
	Dim y As uByte	
	Dim i As Byte
	Dim stairs As uByte
	Dim tileIndex As uByte
	Dim tilePlatformOffset As uByte
	Dim tileBackgroundIndex As uByte
	Dim bigTileIndex As uByte
	
	' First we activate the charset used to draw the back-
	' grounds of hammerin' levels.
	
	Poke uInteger 23606, (@inGameTilesGfxData (0)) - 256
	
	' Borders
	
	tileIndex = (hammeringLevels (nPant, 42) << 2) + 68
	For y = 3 To 21 Step 2
		tilePutBlock (0, y, tileIndex)
		tilePutBlock (30, y, tileIndex)
	Next y
	tilePutHalfBlock (0, 23, tileIndex)
	tilePutHalfBlock (30, 23, tileIndex)
	
	' Upmost background
	
	tileBackgroundIndex = (hammeringLevels (nPant, 41) << 3) + 36
	
	tilePutBlockWithShadow (2, 3, tileBackgroundIndex, 4, 4, 4, 0)
	For x = 4 To 28 Step 2
		tilePutBlockWithShadow (x, 3, tileBackgroundIndex, 4, 4, 0, 0)
	Next x
	tilePutBlockWithShadow (2, 5, tileBackgroundIndex, 4, 0, 4, 0)
	For x = 4 To 28 Step 2
		tilePutBlock (x, 5, tileBackgroundIndex)
	Next x
	
	' Floors
	
	' First we define the two big tiles used on the fly.
	' tile #0 will be without stairs, tile #1 will be with stairs.
	
	tileIndex = hammeringLevels (nPant, 40) * 6
	hammeringBigTiles (0, 1) = tileIndex
	hammeringBigTiles (0, 2) = tileIndex + 1
	hammeringBigTiles (0, 4) = tileBackgroundIndex + 6
	hammeringBigTiles (0, 5) = tileBackgroundIndex + 7
	hammeringBigTiles (0, 6) = tileBackgroundIndex + 6
	hammeringBigTiles (0, 7) = tileBackgroundIndex + 7
	hammeringBigTiles (0, 8) = tileBackgroundIndex
	hammeringBigTiles (0, 9) = tileBackgroundIndex + 1
	hammeringBigTiles (0, 10) = tileBackgroundIndex
	hammeringBigTiles (0, 11) = tileBackgroundIndex + 1
	hammeringBigTiles (0, 12) = tileBackgroundIndex + 2
	hammeringBigTiles (0, 13) = tileBackgroundIndex + 3
	hammeringBigTiles (0, 14) = tileBackgroundIndex + 2
	hammeringBigTiles (0, 15) = tileBackgroundIndex + 3
	
	hammeringBigTiles (1, 1) = 92
	hammeringBigTiles (1, 2) = 93
	hammeringBigTiles (1, 4) = tileBackgroundIndex + 6
	hammeringBigTiles (1, 5) = 92
	hammeringBigTiles (1, 6) = 93
	hammeringBigTiles (1, 7) = tileBackgroundIndex + 7
	hammeringBigTiles (1, 8) = tileBackgroundIndex + 0
	hammeringBigTiles (1, 9) = 92
	hammeringBigTiles (1, 10) = 93
	hammeringBigTiles (1, 11) = tileBackgroundIndex + 5
	hammeringBigTiles (1, 12) = tileBackgroundIndex + 2
	hammeringBigTiles (1, 13) = 92
	hammeringBigTiles (1, 14) = 93
	hammeringBigTiles (1, 15) = tileBackgroundIndex + 7
	
	For y = 0 To 3
	
		' Stairs
		
		stairs = hammeringLevels (nPant, 7 * y)
		For x = 0 To 6
		
			' Select which big tile (w/stairs or wo/stairs) depending on
			' the current bit of "stairs" being processed. Hell, I'm cryptic!
		
			bigTileIndex = stairs - ((stairs >> 1) << 1)
			If bigTileIndex = 0 Then
				curScreenBuf (y, x + x) = 12		' No stairs = platform gauge=12
			Else
				curScreenBuf (y, x + x) = 128		' Stairs.
			End If
		
			' Collapsing floor sections. We modify the big tiles on the fly
			
			If x > 0 Then
				tilePlatformOffset = (3 - hammeringLevels (nPant, 7 * y + x) / 4)
				If tilePlatformOffset < 3 Then
					hammeringBigTiles (bigTileIndex, 0) = tileIndex + 1 + 2 * tilePlatformOffset
					hammeringBigTiles (bigTileIndex, 4) = tileBackgroundIndex + 6
					hammeringBigTiles (0, 5) = tileBackgroundIndex + 7
				Else
					hammeringBigTiles (bigTileIndex, 0) = tileBackgroundIndex
					hammeringBigTiles (bigTileIndex, 4) = tileBackgroundIndex + 2
					hammeringBigTiles (0, 5) = tileBackgroundIndex + 3
				End If
			Else
				hammeringBigTiles (bigTileIndex, 0) = tileIndex + 1
			End If
			
			If x < 6 Then
				tilePlatformOffset = (3 - hammeringLevels (nPant, 7 * y + x + 1) / 4)
				curScreenBuf (y, x + x + 1) = hammeringLevels (nPant, 7 * y + x + 1)
				If tilePlatformOffset < 3 Then
					hammeringBigTiles (bigTileIndex, 3) = tileIndex + 2 * tilePlatformOffset
				Else
					hammeringBigTiles (bigTileIndex, 3) = tileBackgroundIndex + 1
				End If
			Else
				hammeringBigTiles (bigTileIndex, 3) = tileIndex
			End If
		
			' leftmost border shadow over background tiles
			
			If x = 0 Then 
				hammeringBigTiles (bigTileIndex, 8) = tileBackgroundIndex + 4
				hammeringBigTiles (bigTileIndex, 12) = tileBackgroundIndex + 6
			Else
				hammeringBigTiles (bigTileIndex, 8) = tileBackgroundIndex 
				hammeringBigTiles (bigTileIndex, 12) = tileBackgroundIndex + 2
			End If
			
			' Draw the modified tile, and jump to the next bit in "stairs"
			
			tilePutBigBlock (2 + (x << 2), 7 + (y << 2), bigTileIndex)
			stairs = stairs >> 1
		Next x
	
	Next y
	
	' Floor: A simple strip of pointy objects.
	
	For x = 2 To 28 Step 2
		tilePutHalfBlock (x, 23, 94)
	Next x
	
	plPant = nPant

End Sub

Sub setAttr (y As Integer, x As Integer, i As Integer)
	' Paints a screen square with the provided attribute value.

	Poke 22528 + (y << 5) + x, i
End Sub

Sub tilePutBigBlock (x As uByte, y As uByte, tile As uByte)
	Dim xx As uByte
	Dim yy As uByte
	Dim c As Byte
	
	c = 0
	For yy = 0 To 3
		For xx = 0 To 3
			tileIndex = hammeringBigTiles (tile, c)
			myPutChar (x + xx, y + yy, tileIndex)
			c = c + 1
		Next xx
	Next yy
End Sub

Sub tilePutBlock (x As uByte, y As uByte, tileIndex As uByte)
	' This sub puts a 2x2 chars block at (x, y). The chars it prints
	' are correlative, beginning with #tileIndex.

	myPutChar (x, y, tileIndex)
	myPutChar (x + 1, y, tileIndex + 1)
	myPutChar (x, y + 1, tileIndex + 2)
	myPutChar (x + 1, y + 1, tileIndex + 3)
End Sub

Sub tilePutHalfBlock (x As uByte, y As uByte, tileIndex As uByte)
	' This sub puts a 1x2 chars block at (x, y). The chars it prints
	' are correlative, beginning with #tileIndex.

	myPutChar (x, y, tileIndex)
	myPutChar (x + 1, y, tileIndex + 1)
End Sub

Sub tilePutHalfBlockReverse (x As uByte, y As uByte, tileIndex As uByte)
	' This sub puts a 1x2 chars block at (x, y). The chars it prints
	' are correlative, beginning with #tileIndex.

	myPutChar (x, y, tileIndex + 1)
	myPutChar (x + 1, y, tileIndex)
End Sub

Sub tilePutBlockWithShadow (x As uByte, y As uByte, tileIndex As uByte, s0 As uByte, _
	s1 As uByte, s2 As uByte, s3 As uByte)
	' This sub puts a 2x2 chars block at (x, y). The chars it prints
	' are correlative, beginning with #tileIndex. It can be used to
	' paint partially modified 2x2 tiles, for example shaded tiles.
	' Values at s* variables are added to the original tileIndex + n
	' values. In this game, using a 0 will paint the original char
	' and using a 4 will paint the shaded version of such char.

	myPutChar (x, y, tileIndex + s0)
	myPutChar (x + 1, y, tileIndex + 1 + s1)
	myPutChar (x, y + 1, tileIndex + 2 + s2)
	myPutChar (x + 1, y + 1, tileIndex + 3 + s3)
End Sub

Sub myPutChar (x As uInteger, y As uInteger, tileIndex As uInteger)
	' Contains code by LCD modified to fit with my engine/data storage.
	
	dim scr as uInteger
	dim a as uInteger
	a = Peek (@linebuffer + y)
	scr = (a << 5) + x + 16384
	
	adr = Peek (uInteger, 23606) + 256 + (tileIndex << 3)
	
	poke uByte scr, peek (adr)
	poke uByte scr + 256, peek (adr + 1)
	poke uByte scr + 512, peek (adr + 2)
	poke uByte scr + 768, peek (adr + 3)
	poke uByte scr + 1024, peek (adr + 4)
	poke uByte scr + 1280, peek (adr + 5)
	poke uByte scr + 1536, peek (adr + 6)
	poke uByte scr + 1792, peek (adr + 7)
	
	poke uByte 22528 + x + (y << 5), inGameTilesGfxDataAttributes (tileIndex)
End Sub
