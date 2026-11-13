CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -MMD -MP

SRCS = token.cpp scanner.cpp type.cpp ast.cpp ast_clone.cpp parser.cpp print_visitor.cpp type_checker.cpp gencode.cpp optimizer.cpp api.cpp main.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = compiler

.PHONY: all clean test test-gencode

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)
	@echo "Compilación exitosa: ./$(TARGET)"

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(OBJS:.o=.d) $(TARGET) inputs/*_tokens.txt inputs/*_ast.txt
	@echo "Limpieza completada."

test: $(TARGET)
	python3 run_tests.py

test-gencode: $(TARGET)
	python3 run_gencode_tests.py

-include $(OBJS:.o=.d)
