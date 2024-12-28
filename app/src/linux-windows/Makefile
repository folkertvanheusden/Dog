all:
	echo woof
	(cd .. ; rm -rf fathom ; git clone https://github.com/jdart1/Fathom/ fathom)
	(cd ../../include/ ; rm -rf libchess ; git clone https://github.com/folkertvanheusden/libchess.git)
	mkdir build ; cd build ; cmake .. ; make ; mv Dog ../${EXE}
