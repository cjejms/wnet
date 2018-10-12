src := $(wildcard *.proto)

msg: $(src)
	protoc -I=./ --cpp_out=./ $(src)