CXX=clang++

PVS_CFG=./PVS-Studio.cfg
# csv, errorfile, fullhtml, html, tasklist, xml
LOG_FORMAT=tasklist
LOG_FORMAT2=html
PVS_LOG=./project.tasks
PVS_LICENSE=/home/israel/.config/PVS-Studio/PVS-Studio.lic
PVS_HTML=report.html
PVS_PREPARE=./pvs-studio-prepare

CFLAGS=-c
OFLAGS=-g
LDFLAGS=
DFLAGS=
INCLUDES=empollon.hpp asyncserversocket.hpp asyncclientsocket.hpp connection.hpp
SOURCES=empollon.cpp main.cpp asyncserversocket.cpp asyncclientsocket.cpp connection.cpp
OBJECTS=$(SOURCES:.cpp=.o)
IOBJECTS=$(SOURCES:.cpp=.o.PVS-Studio.i)
POBJECTS=$(SOURCES:.cpp=.o.PVS-Studio.log)
EXECUTABLE=test2
CHATGPTSEND=folder2chatgpt
CHATGPTLOAD=chatgpt2folder
VALGRIND=valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 -s

.PHONY: all clean prepare chatgpt_send chatgpt_load

all: $(EXECUTABLE)

prepare: $(SOURCES)
	@$(PVS_PREPARE) -c 1 . > /dev/null 2>&1

$(EXECUTABLE): $(OBJECTS)
# Linking
	@$(CXX) $(LDFLAGS) $(OBJECTS) -pthread -o $@
# Converting
	@plog-converter -a 'GA:1,2' -t $(LOG_FORMAT) $(POBJECTS) -o $(PVS_LOG) > /dev/null 2>&1
	@plog-converter -a 'GA:1,2' -t $(LOG_FORMAT2) $(POBJECTS) -o $(PVS_HTML) > /dev/null 2>&1


$(OBJECTS): %.o: %.cpp prepare
# Build
	@$(CXX) $(CFLAGS) $< $(DFLAGS) $(OFLAGS) -o $@
# Preprocessing
	@$(CXX) $(CFLAGS) $< $(DFLAGS) -E -o $@.PVS-Studio.i > /dev/null 2>&1
# Analysis
	@pvs-studio --lic-file=$(PVS_LICENSE) --cfg $(PVS_CFG) --source-file $< --i-file $@.PVS-Studio.i --output-file $@.PVS-Studio.log > /dev/null 2>&1

clean:
	@rm -f $(OBJECTS) $(IOBJECTS) $(POBJECTS) $(EXECUTABLE) $(PVS_LOG) $(PVS_HTML)

show: $(EXECUTABLE)
	@qutebrowser $(PVS_HTML)

run: $(EXECUTABLE)
	@echo ""
	@echo "Running..."
	@echo "=========="
	@./$(EXECUTABLE)

chatgpt_send:
	@echo ""
	@echo "Enter a message to send to ChatGPT:"
	@echo "==================================="
	@$(CHATGPTSEND) | xclip

chatgpt_load:
	@echo ""
	@echo "Loading code from ChatGPT..."
	@echo "============================"
	@$(CHATGPTLOAD)

valgrind:
	@echo ""
	@echo "Running with valgrind..."
	@echo "========================"
	@$(VALGRIND) ./$(EXECUTABLE)
