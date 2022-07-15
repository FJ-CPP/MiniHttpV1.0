BIN=svr
SRC=main.cc
CC=g++
FLAGS=-std=c++11 -lpthread #C++的thread库也需要-l pthread编译选项
CURR=$(shell pwd)

.PHONY:ALL
ALL:$(BIN) CGI

$(BIN):$(SRC)
	$(CC) -o $@ $^ $(FLAGS)

CGI:
	cd $(CURR)/cgi;\
		make;\
		cd -;

.PHONY:clean
clean:
	rm -rf $(BIN)
	rm -rf release
	cd $(CURR)/cgi;\
		make clean;\
		cd -;

.PHONY:release
release:
	mkdir -p release 
	cp $(BIN) release 
	cp -rf wwwroot release/
	cp ./cgi/test_cgi release/wwwroot
	cp ./cgi/python_cgi.py release/wwwroot
	cp ./cgi/mysql_cgi release/wwwroot

