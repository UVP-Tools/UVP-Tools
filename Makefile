$(shell dos2unix build_tools)
$(shell chmod u+x build_tools)
all:
	@./build_tools -b
clean:
	@./build_tools -c
