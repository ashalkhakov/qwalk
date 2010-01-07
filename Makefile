SDL_CONFIG?=sdl-config

UNIX_LDFLAGS=-g -lm -ldl

CC=gcc
BASE_CFLAGS=-g -MD -Wall -pedantic

CMD_RM=rm -f

OBJECTS=anorms.o image.o image_jpeg.o image_tga.o matrix.o model.o model_md2.o model_md3.o model_mdl.o util.o

help:
	@echo "* make help : Show this message"
	@echo "* make clean : Delete executable, object and dependency files"
	@echo "* make modelconv : Make model converter"
	@echo "* make viewer : Make model viewer"
	@echo "* make all : Make everything"

all:
	$(MAKE) modelconv viewer

modelconv:
	$(MAKE) EXE=modelconv CFLAGS="$(BASE_CFLAGS)" LDFLAGS="$(UNIX_LDFLAGS)" modelconv_

viewer:
	$(MAKE) EXE=viewer CFLAGS="$(BASE_CFLAGS) `$(SDL_CONFIG) --cflags`" LDFLAGS="$(UNIX_LDFLAGS) -lGL `$(SDL_CONFIG) --libs`" viewer_

modelconv_: $(OBJECTS) modelconv.o
	$(CC) -o modelconv $^ $(LDFLAGS)

viewer_: $(OBJECTS) viewer.o
	$(CC) -o viewer $^ $(LDFLAGS)

clean:
	$(CMD_RM) modelconv
	$(CMD_RM) viewer
	$(CMD_RM) *.o
	$(CMD_RM) *.d

