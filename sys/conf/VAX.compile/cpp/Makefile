#	Makefile	1.2	83/07/02
#

CFLAGS=-O -Dunix=1 -DFLEXNAMES -Dpdp11

cpp :	cpp.o cpy.o rodata.o
	$(CC) $(CFLAGS) -o cpp rodata.o cpp.o cpy.o  

cpy.c : cpy.y
	yacc cpy.y
	/usr/src/lib/pcc/:yyfix yyexca yyact yypact yypgo yyr1 yyr2 yychk yydef
	mv y.tab.c cpy.c
cpy.o :	cpy.c yylex.c
rodata.o : cpy.c
	$(CC) $(CFLAGS) -R -c rodata.c

clean :
	rm -f *.s *.o cpy.c rodata.c

install :
	install -s cpp ../CPP

sources : cpp.c cpy.y yylex.c README
cpp.c cpy.y yylex.c README :
	sccs get $@
