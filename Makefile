CXX      := g++
CXXFLAGS := -std=c++14 -Wall -O2 -pthread
LDFLAGS  := -pthread

# Windows (MinGW): link against Winsock2
ifeq ($(OS),Windows_NT)
  LDFLAGS += -lws2_32
  EXE := .exe
else
  EXE :=
endif

SRC_DIR := src
BIN_DIR := bin

SRC_FILES := $(SRC_DIR)/main.cpp \
             $(SRC_DIR)/load_balancer.cpp \
             $(SRC_DIR)/backend_pool.cpp \
             $(SRC_DIR)/health_checker.cpp \
             $(SRC_DIR)/connection_handler.cpp \
             $(SRC_DIR)/http_parser.cpp

BACKEND_SRC := backend/server.cpp

.PHONY: all clean run-backends stop-backends run-lb

all: $(BIN_DIR)/load_balancer$(EXE) $(BIN_DIR)/backend_server$(EXE)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/load_balancer$(EXE): $(SRC_FILES) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC_FILES) $(LDFLAGS)
	@echo "Built: $@"

$(BIN_DIR)/backend_server$(EXE): $(BACKEND_SRC) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(BACKEND_SRC) $(LDFLAGS)
	@echo "Built: $@"

run-backends: $(BIN_DIR)/backend_server$(EXE)
	@echo "Starting backends on 9001, 9002, 9003..."
	@$(BIN_DIR)/backend_server$(EXE) 9001 &
	@$(BIN_DIR)/backend_server$(EXE) 9002 &
	@$(BIN_DIR)/backend_server$(EXE) 9003 &

stop-backends:
	@pkill -f "backend_server" 2>/dev/null || taskkill /F /IM backend_server.exe 2>/dev/null || true

run-lb: $(BIN_DIR)/load_balancer$(EXE)
	./$(BIN_DIR)/load_balancer$(EXE) --config config.json

clean:
	rm -rf $(BIN_DIR)
