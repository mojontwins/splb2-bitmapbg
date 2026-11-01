Check BE99ADD6
Auto 1
   1 REM : INT + anim,sadd,smsb,slsb,dadd,dmsb,add,np,cminit,cmupd
   2 REM : INT pox,poy,pscore,pbon,pwat,ax,ay,px,py,psal,pnu,pfacing,i,j,plife,pfal
   3 REM :\#186x,y,gv1,gv2,gv3,xx,yy
   4 REM : LEN q$\#19912,p$\#19912,b$\#19996,t$\#1992,$\#19932
   5 REM : INT FN v
   6 REM :\#186nb,ex,ey,esal,enu,estate,ect,ernd
   9 REM : GO TO 500,520,540,560,580,600,620,640,660,680,700,720
  10 REM : GO SUB 510,530,550,570,590,610,630,650,670,690,710,730
  30 REM : OPEN #
  50 DEF FN v(t$)=10*(CODE t$(1)-48)+CODE t$(2)-48
  60 LET cminit=VAL "54596": LET cmupd=VAL "54736"
 100 POKE 23606,88: GO SUB 9998
 110 BORDER 0: POKE 23624,1: INK 6: PAPER 0: BRIGHT 0: CLS
 120 GO SUB 6000: GO TO 1000
 150 LET smsb=INT (sadd/256): LET slsb=sadd-256*smsb: LET dmsb=INT (dadd/256): POKE 54095,slsb: POKE 54096,smsb: POKE 54097,0: POKE 54098,dmsb: RANDOMIZE USR 54099: RETURN
 200 LET pfal=psal: IF psal=1\{vn} THEN GO TO 220
 210 IF py<22 THEN IF ATTR (py+2,px)<64 AND ATTR (py+2,px+1)<64 THEN LET py=py+1: LET pfal=1
 220 IF IN 57342=253 OR IN 57342=189 THEN IF px>0 THEN IF ATTR (py,px-1)<64 AND ATTR (py+1,px-1)<64 THEN LET px=px-1: POKE 54400,4: POKE 54401,5: POKE 54402,6: POKE 54403,7: IF NOT pfal THEN RANDOMIZE USR 53742
 230 IF IN 57342=254 OR IN 57342=190 THEN IF px<30 THEN IF ATTR (py,px+2)<64 AND ATTR (py+1,px+2)<64 THEN LET px=px+1: POKE 54400,0: POKE 54401,1: POKE 54402,2: POKE 54403,3: IF NOT pfal THEN RANDOMIZE USR 53742
 240 IF IN 64510=254 OR IN 64510=190 THEN IF psal=0 THEN IF ATTR (py+2,px)>=64 OR ATTR (py+2,px+1)>=64 OR py=22 THEN LET psal=1: LET pnu=0: RANDOMIZE USR 53959: GO TO 280
 250 IF psal=0 THEN GO TO 280
 260 IF py>0 THEN IF ATTR (py-1,px)<64 AND ATTR (py-1,px+1)<64 THEN LET py=py-1
 270 LET pnu=pnu+1: IF pnu=5 THEN LET psal=0
 280 POKE 54404,px: POKE 54405,py
 290 RETURN
 300 INK 1: CLS : LET add=54059+np+np+np: LET dadd=16384: FOR i=0 TO 2
 310 IF PEEK (add+i)=1 THEN LET sadd=62438: GO TO 370
 320 IF PEEK (add+i)=2 THEN LET sadd=61011: GO TO 370
 330 IF PEEK (add+i)=3 THEN LET sadd=59415: GO TO 370
 340 IF PEEK (add+i)=4 THEN LET sadd=58172: GO TO 370
 350 IF PEEK (add+i)=5 THEN LET sadd=56790: GO TO 370
 360 IF PEEK (add+i)=6 THEN LET sadd=55197
 370 GO SUB 150: LET dadd=dadd+2048: NEXT i
 380 RETURN
 390 FOR j=0 TO 11: LET np=j: GO SUB 300: PRINT AT 0,0;j;" ";add: PAUSE 0: NEXT j
 400 GO SUB 300: GO TO 500+np*20
 410 PRINT #0;AT 0,0; BRIGHT 1; INK 4;"[\\[\\[\\[\\[\\[\\[\\[\\[\\[\\[\\[\\[\\[\\[\\[\\"; INK 2;"]^]^]^]^]^]^]^]^]^]^]^]^]^]^]^]^";: RETURN
 420 FOR i=1 TO LEN b$ STEP 4: LET yy=FN v(b$(i TO i+1)): LET xx=FN v(b$(i+2 TO i+3)): PRINT AT yy,xx;p$;AT yy+1,xx;q$: NEXT i: RETURN
 500 GO SUB 410
 501 PRINT AT 21,0;"\{b1i5}qrqrqrqrqrqrqrqrqr";AT 20,0;"\{b1i5}rqrqrqrqrqrqrqrqr": FOR i=8 TO 19 STEP 2: PRINT AT i,0;"\{b1i5}rq\{b0i6}, , , , , , , f";AT i+1,0;"\{b1i5}qr\{b0i6} , , , , , , ,f": NEXT i: PRINT AT 7,0;"\{b1i5}qrqrqrqrqrqrqrqrqrqr";AT 6,0;"\{b1i2}RRRRRRRRRRRRRRRRRRRR";AT 5,0;"\{b1i2}RRRRRRRRRRRRRRRRRRRR";AT 3,5;"\{b1i5}gj";AT 4,5;"\{b1i5}kn"
 502 PRINT AT 17,3;"\{b1i3}\a\b";AT 18,2;"\{b1i2}{\{i3}ST\{b0i6}  \{b1i2}\*";AT 19,2;"\{b1i2}|\{b0i7}}}\{p1i6b1}~~\{p0i2}|";AT 14,2;"\{b1i5}rqrqrqrqrqr";AT 13,2;"\{b1i2}|\{i7}}}\{p1i6}~~\{p0i2}|";AT 12,2;"\{b1i2}{\{b0i6}    \{i2b1}\*";AT 18,14;"\{b1i2}D\{i3}E";AT 19,14;"\{b1i2}D\{i3}E"
 503 PRINT AT 18,10;"\{b0i7}Q";AT 19,10;"\{b0i7}R";
 508 LET gv1=0
 509 RETURN
 510 PRINT AT 17,10;("\{b0i2}M" AND gv1=0);("\{b0i6}N" AND gv1=1);("\{b0i2}O" AND gv1=2);("\{b0i6}P" AND gv1=3)
 511 LET gv1=gv1+1: IF gv1=4 THEN LET gv1=0
 519 RETURN
 520 GO SUB 410: LET p$="\{b1i6}\@\{i4}A\{i6}\@\{i4}A": LET q$="\{b1i4}BCBC": LET b$="2004180716081208100708090610071004110511021003100009": GO SUB 420
 521 LET p$="\{b1i2}_\`": LET q$="\{b1i2}a\{i3}b": LET b$="1821142410281030": GO SUB 420
 522 PRINT AT 19,16;"\{i7b1}st";AT 20,16;"\{b1i7}uv";AT 21,15;"\{b1i5}wxyz"
 529 RETURN
 530 LET ax=22: LET ay=5: GO SUB 750
 539 RETURN
 540 GO SUB 410: LET p$="\{b1i2}_\`": LET q$="\{b1i2}a\{i3}b": PRINT AT 10,0;"\{b1i2}_\`_\`_\`_\`_\`_\`_\`_\`_\`_\`_\`_\`";AT 11,0;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$
 541 LET p$="\{b1i7}I\{i5}J": LET q$="\{b1i5}KL": LET b$="1024112815251927": GO SUB 420: FOR i=6 TO 20 STEP 2: PRINT AT i,30;p$;AT i+1,30;q$: NEXT i
 542 PRINT AT 21,6;"\{b1i5}qr\{i2}_\`_\`_\`_\`_\`_\`_\`_\`\{i5}qr"
 549 RETURN
 550 LET ax=16: LET ay=6: GO SUB 750
 559 RETURN
 560 GO SUB 410: LET p$="\{b1i7}I\{i5}J": LET q$="\{b1i5}KL": LET b$="1002160213061907": GO SUB 420: FOR I=6 TO 20 STEP 2: PRINT AT i,0;p$;AT i+1,0;q$: NEXT i: LET p$="\{b1i4}q\{i5}r": LET q$="\{b1i5}r\{i4}q": LET b$="1112112620142026": GO SUB 420
 561 PRINT AT 12,14;"\{b1i2}]^]^]^]^]^]^";AT 21,16;"_\`_\`_\`_\`_\`";AT 13,12;p$;p$;p$;p$;p$;p$;p$;p$;AT 21,6;p$;p$;
 569 RETURN
 570 LET ax=23: LET ay=4: GO SUB 750
 579 RETURN
 580 PRINT #0;AT 0,0; BRIGHT 1; INK 4;"[\\[\\[\\[\\[\\[\\";AT 0,20;"[\\[\\[\\[\\[\\[\\"; INK 2;"]^]^]^]^]^]^";AT 1,20;"]^]^]^]^]^]^";
 581 LET p$="\{b1i6}I\{i5}J": LET q$="\{b1i5}KL": LET b$="2004180620062008202218242024202613091321140714230716": GO SUB 420: LET p$="\{b1i2}]\{i3}^": LET q$="\{b1i4}q\{i5}r": PRINT AT 9,11;p$;p$;p$;p$;p$;AT 10,11;q$;q$;q$;q$;q$;AT 4,22;p$;p$;p$;p$;AT 5,22;q$;q$;q$;q$;: LET p$="\{b1i6}\@\{i4}A\@A": LET q$="\{b1i4}BCB\{i5}C": LET b$="0000000402000102": GO SUB 420
 589 RETURN
 590 LET ax=11: LET ay=7: GO SUB 750
 599 RETURN
 600 GO SUB 410: LET p$="\{b1i6}\@\{i4}A\@A": LET q$="B\{i5}C\{i4}B\{i5}C": LET b$="001800240218022002240228042004260626082510231224122814281626182618232024": GO SUB 420: LET b$="002200280428062808281028162818282028": GO SUB 420: LET p$="\{b1i6}I\{i5}J": LET q$="\{b1i5}KL": LET b$="07080818121316102006": GO SUB 420
 601 FOR i=0 TO 4 STEP 2: PRINT AT i,2;p$;AT i+1,2;q$: NEXT i: PRINT AT 4,4;"\{b0i2}bab";AT 3,4;"\{b1i3}_\`_";AT 6,3;"_\`_\`_";AT 7,3;"\{b1i4}qrqrqr"
 609 RETURN
 610 LET ax=8: LET ay=14: GO SUB 750
 619 RETURN
 620 GO SUB 410: LET p$="\{b1i2}r\{i3}q": LET q$="\{b1i3}q\{i2}r": PRINT AT 0,0;p$;p$;p$;;p$;p$;p$;p$;p$;p$;p$;p$;p$;p$;p$;p$;p$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$: FOR i=2 TO 20 STEP 2: PRINT AT i,0;p$;AT i+1,0;q$;: NEXT i: FOR i=16 TO 20 STEP 2: PRINT AT i,12;p$;AT i+1,12;q$: NEXT i
 621 LET p$="\{b1i2}_\`": LET q$="\{b1p1i2}ab"\{p0}: PRINT AT 12,2;p$;p$;p$;AT 13,2;q$;q$;q$;AT 20,2;p$;p$;p$;p$;p$;AT 21,2;q$;q$;q$;q$;q$;AT 20,14;p$;p$;p$;p$;p$;AT 21,14;q$;q$;q$;q$;q$;
 622 LET p$="\{p0b1i6}I\{i5}J": LET q$="\{b1i5}KL": LET b$="120812161418162018222024": GO SUB 420: PRINT AT 10,9;"\{b0i7}Q";AT 10,16;"\{b0i7}Q";AT 11,9;"\{b0i7}R";AT 11,16;"\{b0i7}R": LET gv1=0
 629 RETURN
 630 PRINT AT 9,9;("\{b0i2}M" AND gv1=0);("\{b0i6}N" AND gv1=1);("\{b0i2}O" AND gv1=2);("\{b0i6}P" AND gv1=3)
 631 PRINT AT 9,16;("\{b0i2}M" AND gv1=0);("\{b0i6}N" AND gv1=1);("\{b0i2}O" AND gv1=2);("\{b0i6}P" AND gv1=3)
 632 LET gv1=gv1+1: IF gv1=4 THEN LET gv1=0
 639 RETURN
 640 LET p$="\{b1i4}[\\": PRINT #0;AT 1,0;"\{b0i2b1}]^\{b0i5}efeffeef\{b1i2}]^\{b0i5}feefffeeef\{b1i2}]^]^]^]^]^";AT 0,0;p$;AT 0,10;p$;AT 0,22;p$;p$;p$;p$;p$;
 641 LET p$="\{b1i2}r\{i3}q": LET q$="\{b1i3}q\{i2}r": PRINT AT 0,0;p$;p$;p$;;p$;p$;p$;p$;p$;p$;p$;p$;p$;p$;p$;p$;p$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$: FOR i=2 TO 4 STEP 2: PRINT AT i,30;p$;AT i+1,30;q$;: NEXT i
 642 LET p$="\{i6b1}I\{i4}J": LET q$="\{b1i4}K\{i5}L": LET b$="04120610072006220630100812101220142216242026": GO SUB 420: LET p$="\{i3b1}_\{b0}\`": LET q$="\{b1i3}q\{i2}r": PRINT AT 12,12;p$;p$;p$;p$;AT 13,12;q$;q$;q$;q$;AT 6,24;p$;p$;p$;AT 7,24;q$;q$;q$
 649 RETURN
 650 LET ax=5: LET ay=6: GO SUB 750
 659 RETURN
 660 LET p$="\{b1i2}r\{i3}q": LET q$="\{b1i3}q\{i2}r": PRINT AT 0,0;p$;p$;p$;;p$;p$;p$;p$;p$;p$;p$;p$;p$;p$;p$;p$;p$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;AT 12,20;p$;p$;p$;p$;p$;p$;AT 13,20;q$;q$;q$;q$;q$;q$
 661 LET p$="\{b1i4}[\\": LET q$="\{b0i2b1}]^": PRINT #0;AT 0,0;p$;p$;p$;p$;p$;p$;p$;AT 0,28;p$;p$;q$;q$;q$;q$;q$;q$;q$;AT 1,28;q$;q$
 662 LET p$="\{i6b1}I\{i4}J": LET q$="\{b1i4}K\{i5}L": LET b$="061806280802081204000600020012181417161618152014": GO SUB 420: LET p$="\{i3b1}_\{b0}\`": LET q$="\{b1i3}q\{i2}r": PRINT AT 6,20;p$;p$;p$;p$;AT 7,20;q$;q$;q$;q$;AT 8,4;p$;p$;p$;p$;AT 9,4;q$;q$;q$;q$: LET p$="\{b1i6}q\{i4}r": LET q$="": LET b$="16182016": GO SUB 420
 669 RETURN
 670 LET ax=9: LET ay=5: GO SUB 750
 679 RETURN
 680 LET p$="\{b1i2}r\{i3}q": LET q$="\{b1i3}q\{i2}r": PRINT AT 0,0;p$;p$;p$;p$;p$;p$;AT 0,20;p$;p$;p$;p$;p$;p$;;q$;q$;q$;q$;q$;q$;AT 1,20;q$;q$;q$;q$;q$;q$;AT 12,0;p$;p$;p$;p$;p$;p$;p$;p$;AT 13,0;q$;q$;q$;q$;q$;q$;q$;q$
 681 LET p$="\{b1i4}[\\": PRINT #0;AT 1,0;"\{b0i2b1}]^]^]^]^\{i5b0}efeffeeffe\{i2}]^]^]^]^]^]^]^";AT 0,0;p$;p$;p$;p$;AT 0,18;p$;p$;p$;p$;p$;p$;p$
 682 LET p$="\{b1i7}I\{i6}J": LET q$="\{b1i6}K\{i4}L": LET b$="031505210708081007200818161820192021": GO SUB 420: LET p$="\{b1i3}_\{i2}\`": LET q$="\{b1i5}qr": PRINT AT 6,2;p$;p$;p$;AT 7,2;q$;q$;q$;AT 7,22;p$;p$;p$;p$;AT 8,22;q$;q$;q$;q$
 689 RETURN
 690 LET ax=23: LET ay=12: GO SUB 750
 699 RETURN
 700 LET p$="\{b1i6}\@\{i4}A\@B": LET q$="\{b1i4}BCB\{i5}C": LET b$="0222022604230427062006240628082610271228": GO SUB 420: LET p$="\{b1i6}\@\{i4}A": LET q$="\{b1i4}B\{i5}C": LET b$="02300830": GO SUB 420: LET p$="\{i6b1}A": LET q$="\{b1i4}C": LET b$="04311031": GO SUB 420
 701 GO SUB 410: LET p$="\{b1i2}r\{i3}q": LET q$="\{b1i3}q\{i2}r": PRINT AT 0,0;p$;p$;p$;;p$;p$;p$;p$;p$;p$;p$;p$;p$;p$;p$;p$;p$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;q$;
 702 LET p$="\{b1i2}D\{b0}E": FOR i=14 TO 21: PRINT AT i,30;p$;: NEXT i
 703 LET p$="\{b1i2p4}ab": LET q$="\{b1p0i4}ab": PRINT AT 6,4;p$;p$;p$;p$;"\{p4i2b1}a";AT 7,4;q$;q$;q$;q$;"\{b1p0i4}a": LET p$="\{i5b1}qr": LET q$="": LET b$="07181116": GO SUB 420: LET p$="\{b1i6}I\{i5}J": LET q$="\{b1i5}KL": LET b$="0602061318072006": GO SUB 420
 704 PRINT AT 13,21;"\{b1i7}gj";AT 14,21;"\{b0i7}kn";AT 15,12;"\{b1i2}RRRRRRRRRRRR";AT 16,12;"\{b0i2}RRRRRRRRRRRR";AT 17,12;"\{b1i5}qrqrqrqrqrqr";: FOR i=18 TO 20 STEP 2: PRINT AT i,12;"\{b0i6}f, , , , , f";AT i+1,12;"\{b0i6}f , , , , ,f": NEXT i
 705 PRINT AT 20,13;"\{i7}Q";AT 21,13;"R";AT 20,16;"\{b1i2}{";AT 20,21;"\{i2b1}\*";AT 21,16;"\{b1i2}|\{i7}}}\{p1i6}~~\{p0i2}|"
 706 LET gv1=0
 709 RETURN
 710 PRINT AT 19,13;("\{b0i2}M" AND gv1=0);("\{b0i6}N" AND gv1=1);("\{b0i2}O" AND gv1=2);("\{b0i6}P" AND gv1=3)
 711 LET gv1=gv1+1: IF gv1=4 THEN LET gv1=0
 719 RETURN
 720 GO SUB 410: LET p$="\{b1i2}r\{i3}q": LET q$="\{b1i3}q\{i2}r": PRINT AT 0,0;p$;p$;p$;p$;p$;p$;p$;AT 0,28;p$;p$;AT 1,0;q$;q$;q$;q$;q$;q$;q$;AT 1,28;q$;q$: FOR i=2 TO 20 STEP 2: PRINT AT i,0;p$;AT i,30;p$;AT i+1,0;q$;AT i+1,30;q$;: NEXT i
 721 LET p$="\{b1i6}I\{i4}J": LET q$="\{b1i4}K\{i5}L": LET b$="03220426072408140816092312201410151616182020": GO SUB 420
 722 LET p$="\{i3b1}_\`": LET q$="\{b1i5}qr": PRINT AT 6,2;p$;p$;p$;p$;p$;AT 7,2;q$;q$;q$;q$;q$;AT 13,2;p$;p$;p$;p$;AT 14,2;q$;q$;q$;q$;AT 18,5;"\{b0i7}cd";AT 19,5;"\{b0i7}ef";AT 20,4;"\{b1i3}gh\{i2}ij";AT 21,4;"\{b1i3}k\{i2}lmn"
 729 RETURN
 730 LET ax=6: LET ay=10: GO SUB 750
 739 RETURN
 750 LET gv2=gv2-1
 755 IF gv3=1 THEN GO TO 760
 756 IF gv2=0 THEN LET gv2=INT (RND*20)+10: LET gv3=1
 757 GO TO 770
 760 IF gv2=0 THEN LET gv2=3+INT (RND*2)+1: LET gv3=0
 770 PRINT AT ay,ax;("\{p0i1b0}--" AND gv3=0);("\{b0p0i2}\*\*" AND gv3=1): RETURN
 780 LET add=53598+12*np: FOR i=0 TO 3: LET j=PEEK (add+2)
 781 IF j=0 THEN LET p$="\{i6}\*": GO TO 791
 782 IF j=1 THEN LET p$="\{i5}W": GO TO 791
 783 IF j=2 THEN LET p$="\{i4}W": GO TO 791
 784 IF j=3 THEN LET p$="\{i4}X": GO TO 791
 785 IF j=4 THEN LET p$="\{i4}Y": GO TO 791
 786 IF j=5 THEN LET p$="\{i6}Y": GO TO 791
 787 IF j=6 THEN LET p$="\{i2}Y": GO TO 791
 788 IF j=7 THEN LET p$="\{i3}Y": GO TO 791
 789 IF j=8 THEN LET p$="\{i3}Z": GO TO 791
 790 IF j=9 THEN LET p$=" "
 791 IF PEEK add<>0 THEN PRINT BRIGHT 0;AT PEEK add,PEEK (add+1);p$
 793 LET add=add+3: NEXT i: RETURN
 800 POKE 23670,PEEK 23672: IF ect>0 THEN LET ect=ect-1: GO TO 820
 805 LET ernd=INT (RND*25): LET ect=3+INT (RND*7)
 806 IF ernd<2 THEN LET estate=1: GO TO 820
 807 IF ernd<4 THEN LET estate=2: GO TO 820
 808 IF ernd<6 THEN LET estate=3: GO TO 820
 810 LET estate=0
 820 IF esal=1 THEN GO TO 840
 830 IF ey<22 THEN IF ATTR (ey+2,ex)<64 AND ATTR (ey+2,ex+1)<64 THEN LET ey=ey+1
 840 IF estate=1 THEN IF ex>0 THEN IF ATTR (ey,ex-1)<64 AND ATTR (ey+1,ex-1)<64 THEN LET ex=ex-1
 850 IF estate=2 THEN IF ex<30 THEN IF ATTR (ey,ex+2)<64 AND ATTR (ey+1,ex+2)<64 THEN LET ex=ex+1
 860 IF estate=3 THEN IF esal=0 THEN IF ATTR (ey+2,ex)>=64 OR ATTR (ey+2,ex+1)>=64 OR ey=22 THEN LET esal=1: LET enu=0: LET estate=0: LET ect=0: GO TO 890
 865 IF esal=0 THEN GO TO 890
 870 IF ey>0 THEN IF ATTR (ey-1,ex)<64 AND ATTR (ey-1,ex+1)<64 THEN LET ey=ey-1
 880 LET enu=enu+1: IF enu=5 THEN LET esal=0
 890 POKE 54452,ex: POKE 54453,ey: RETURN
 900 POKE 23670,PEEK 23672: LET estate=0: LET ect=0: LET ernd=0: LET esal=0: LET enu=0
 901 LET ex=INT (RND*30): LET ey=INT (RND*20): IF ATTR (ey,ex)>=64 OR ATTR (ey+1,ex)>=64 OR ATTR (ey,ex+1)>=64 OR ATTR (ey+1,ex+1)>=64 THEN GO TO 901
 902 POKE 54452,ex: POKE 54453,ey
 903 LET nb=INT (RND*3): POKE 54448,8+4*nb: POKE 54449,9+4*nb: POKE 54450,10+4*nb: POKE 54451,11+4*nb: LET nb=INT (RND*7)+1: POKE 54488,64+nb: POKE 54489,nb: POKE 54490,nb: POKE 54491,nb: RETURN
1000 LET pscore=0: LET pbon=0: LET pwat=0: LET add=53600: FOR i=0 TO 47: POKE add,0: LET add=add+3: NEXT i: LET plife=20: LET np=10: LET px=18: LET py=10: LET psal=0: LET pnu=0
1010 POKE 54400,0: POKE 54401,1: POKE 54402,2: POKE 54403,3: POKE 54440,7: POKE 54441,6: POKE 54442,6: POKE 54443,6
1020 POKE 54496,99: POKE 54544,99
1025 LET pox=px: LET poy=py: GO SUB 9999: LET anim=510+20*np: GO SUB 400: GO SUB 900: POKE 54454,ex: POKE 54455,ey
1030 GO SUB 780: LET gv3=0: LET gv2=10: POKE 54404,px: POKE 54405,py: POKE 54406,px: POKE 54407,py: RANDOMIZE USR cminit
1031 IF pwat>0 THEN GO SUB 2000
1032 IF pbon>0 THEN GO SUB 2010
1035 POKE 23673,0
1040 GO SUB 4000: GO SUB 510+np*20: GO SUB 800: GO SUB 200: RANDOMIZE USR cmupd: IF pbon AND py<=21 THEN PRINT AT py,px;"\{i3}op"
1050 IF px=0 THEN IF (IN 57342=253 OR IN 57342=189) THEN LET np=np-1: LET px=30: GO TO 1025
1051 IF px=30 THEN IF (IN 57342=254 OR IN 57342=190) THEN LET np=np+1: LET px=0: GO TO 1025
1052 IF py=0 AND psal=1 AND np=9 THEN LET pnu=0: LET py=23: LET np=4: GO TO 1025
1053 IF py=0 AND psal=1 AND np=11 THEN LET pnu=pnu-1: LET py=23: LET np=8: GO TO 1025
1054 IF py=22 AND psal=0 AND np=4 THEN LET np=9: LET py=0: GO TO 1025
1055 IF py=22 AND psal=0 AND np=8 THEN LET np=11: LET py=0: GO TO 1025
1060 IF ex>=px-1 AND ex<=px+1 AND ey>=py-1 AND ey<=py+1 THEN RANDOMIZE USR 53809: LET plife=plife-1: GO SUB 900: RANDOMIZE USR 54736: GO SUB 2020
1070 IF INKEY$=" " THEN GO SUB 3000
1080 IF pscore=30 THEN GO TO 7000
1085 IF py=21 THEN IF ATTR (py+2,px)=5 OR ATTR (py+2,px+1)=5 THEN RANDOMIZE USR 53809: LET plife=plife-1: LET px=pox: LET py=poy: POKE 54404,px: POKE 54405,py: RANDOMIZE USR 54736: GO SUB 2020
1090 LET j=0: FOR i=0 TO 500: LET j=j+1: NEXT i
1095 IF plife=0 THEN GO TO 2030
1100 GO TO 1040
2000 GO SUB 9998: PRINT #0;AT 1,29;"\{i5}W=";pwat: GO SUB 9999: RETURN
2010 LET x=1: FOR i=1 TO pbon: PRINT #0;AT 1,x;"\{p0i3}op": LET x=x+3: NEXT i: RETURN
2020 PRINT #0;AT 1,0;"\{i6}\u=";plife: RETURN
2030 PRINT AT 11,11;"GAME OVER!": FOR i=0 TO 10: RANDOMIZE USR 54009: NEXT i: PAUSE 0: GO TO 100
3000 IF np=11 THEN IF px=5 AND py=18 THEN RANDOMIZE USR 54009: LET pwat=5: GO SUB 2000: RETURN
3010 LET add=53598+12*np: FOR i=0 TO 3: IF PEEK add=py+1 AND (PEEK (add+1)=px OR PEEK (add+1)=px+1) THEN GO SUB 3500
3020 LET add=add+3: NEXT i
3030 IF np=1 THEN IF px=16 AND py=17 THEN IF pbon THEN RANDOMIZE USR 53759: LET pscore=pscore+pbon: LET pbon=0: GO SUB 410: GO SUB 9998: PRINT #0;AT 1,11; BRIGHT 1; INK 7;"BONIATOS:";pscore: GO SUB 9999
3040 IF np=1 THEN IF px=16 AND py=17 THEN IF NOT pbon THEN GO SUB 9998: PRINT #0;AT 1,11; BRIGHT 1; INK 7;"BONIATOS:";pscore: GO SUB 9999
3499 RETURN
3500 IF PEEK (add+2)=0 THEN RANDOMIZE USR 53759: POKE add+2,1: RANDOMIZE USR 54750: PRINT AT PEEK add,PEEK (add+1);"\{i5}W": RANDOMIZE USR 54596: RANDOMIZE USR 54877: RETURN
3510 IF PEEK (add+2)=1 AND pwat>0 THEN LET pwat=pwat-1: GO SUB 2000: RANDOMIZE USR 53859: POKE add+2,2: RANDOMIZE USR 54750: PRINT AT PEEK add,PEEK (add+1);"\{i4}W": RANDOMIZE USR 54596: RANDOMIZE USR 54877: RETURN
3520 IF PEEK (add+2)>4 AND PEEK (add+2)<8 THEN IF pbon<3 THEN RANDOMIZE USR 53909: POKE add+2,9: RANDOMIZE USR 54750: PRINT AT PEEK add,PEEK (add+1);" ": RANDOMIZE USR 54596: RANDOMIZE USR 54877: LET pbon=pbon+1: GO SUB 2010: RETURN
3599 RETURN
4000 IF PEEK 23673<3 THEN RETURN
4010 POKE 23673,0
4020 LET add=53600: FOR i=0 TO 47: IF PEEK add>1 AND PEEK add<8 THEN POKE add,1+PEEK add
4030 LET add=add+3: NEXT i: RANDOMIZE USR 54750: GO SUB 780: RANDOMIZE USR 54596: RANDOMIZE USR 54877: RETURN
5000 LET i=0
6000 LET sadd=52848: LET dadd=20480: GO SUB 150: PRINT AT 21,0;: FOR i=0 TO 16: POKE 23692,255: PRINT : NEXT i
6001 FOR i=8 TO 15: PRINT AT i,0; INK 1;,,;: NEXT i: LET sadd=55197: LET dadd=18432: GO SUB 150
6010 PRINT AT 10,8;"                ";AT 11,8;" \{i7}O \{i4}LEFT\{i2},\{i7}P \{i4}RIGHT ";AT 12,8;"\{i7}Q \{i4}JUMP\{i2},\{i7}SP \{i4}ACTION";AT 13,8;"                "
6015 PRINT AT 19,6;"\{i3}FOURSPRITER \{i5}SHOWCASE"
6020 PRINT AT 21,8;"\{i5}ANY KEY TO PLAY!"
6030 RANDOMIZE USR 51720: RETURN
6040 PRINT AT 17,0;: LET x=0: FOR i=1 TO LEN b$: PRINT b$(i);((CHR$ 13+CHR$ 13) AND x=31);: LET x=x+1: PAUSE 4: NEXT i: PAUSE 100: PRINT AT 17,0;,,,,,,;: RETURN
7000 GO SUB 9998: INK 5: CLS : INK 7: LET sadd=52267: LET dadd=18432: GO SUB 150
7010 LET B$=" GREAT! YOU MADE IT! YOU         COLLECTED ENOUGH BONIATOS      ": GO SUB 6040
7020 LET B$=" GRAND BOLOTE IS NOW CURED SO    YOU CAN ESCAPE THE ISLAND...    ": GO SUB 6040
7030 LET B$=" SEE YOU IN FUTURE ADVENTURES!   STAY TUNED TO MOJONIA FM       ": GO SUB 6040
7040 PAUSE 0: GO TO 100
9997 STOP
9998 POKE 23607,251: RETURN
9999 POKE 23607,248: RETURN
