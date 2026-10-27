CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

SRCS = token.cpp scanner.cpp type.cpp ast.cpp parser.cpp print_visitor.cpp type_checker.cpp main.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = compiler

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)
	@echo "Compilación exitosa: ./$(TARGET)"

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) inputs/*_tokens.txt inputs/*_ast.txt
	@echo "Limpieza completada."

test: $(TARGET)
	python3 run_tests.py
