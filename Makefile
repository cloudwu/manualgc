#
#	Copyright (c) 2008 ,                                                      
#		Cloud Wu . All rights reserved.                                       
#                                                                            
#		http://www.codingnow.com                                              
#                                                                            
# 	Use, modification and distribution are subject to the "New BSD License"   
#	as listed at <url: http://www.opensource.org/licenses/bsd-license.php >.  
#
#	For MSVC, use CC = cl /o , or CC = gcc -o for gcc
#

CC= cl /o
#CC= gcc -o

#RM= rm
RM= del

test.exe: test.c gc.c
	$(CC) $@ $^

clean:
	$(RM) *.obj *.o *.exe