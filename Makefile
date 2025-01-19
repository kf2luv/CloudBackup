# 目标文件
TARGET = bin/cloud

# 编译器与选项
CXX = g++
CXXFLAGS = -std=c++17 -I./include
LDFLAGS = -ljsoncpp -lpthread -lstdc++fs -lmysqlcppconn -L./lib -lbundle

# 源文件
SOURCES = src/main.cc

# 目标规则
$(TARGET): $(SOURCES)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS)

# 清理规则
clean:
	rm -f $(TARGET)
