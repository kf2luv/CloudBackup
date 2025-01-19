# 目标文件
TARGET = cloud

# 编译器与选项
CXX = g++
CXXFLAGS = -std=c++17
LDFLAGS = -ljsoncpp -lpthread -lstdc++fs -lbundle -lmysqlcppconn

# 源文件
SOURCES = main.cc

# 目标规则
$(TARGET): $(SOURCES)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS)

# 清理规则
clean:
	rm -f $(TARGET)
