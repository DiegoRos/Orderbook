# =============================================================================
# Makefile for the Orderbook Project
# Generated from a .vcxproj file for a Linux environment.
#
# Common Commands:
#   make          - Builds the release version of the project.
#   make release  - Explicitly builds the release version.
#   make debug    - Builds the debug version with debug symbols.
#   make clean    - Removes all generated build files.
# =============================================================================

# --- Compiler and Linker ---
# Use g++ as the C++ compiler. You can change this to clang++ if you prefer.
CXX = g++-13

# --- Project Files ---
# The name of the final executable, based on <ProjectName>
TARGET = Orderbook

# List of all C++ source files
SRCS = \
	Orderbook.cpp\
	# main.cpp 

# List of all header files.
# Used for explicit dependency tracking if needed, though the automatic dependency
# generation below is generally sufficient.
HEADERS = \
	Constants.h \
	LevelInfo.h \
	Order.h \
	Orderbook.h \
	OrderbookLevelInfos.h \
	OrderModify.h \
	OrderType.h \
	Side.h \
	Trade.h \
	TradeInfo.h \
	Usings.h

# Automatically generate object file names by replacing .cpp with .o
OBJS = $(SRCS:.cpp=.o)

# --- Build Flags ---
# Common flags used for all build types.
CXXFLAGS_COMMON = -std=c++20

# Flags for the Release build.
# -O3 for high optimization, -Wall for standard warnings.
# -DNDEBUG is the standard flag to disable asserts and debug code.
# -flto enables Link-Time Optimization.
CXXFLAGS_RELEASE = -O3 -Wall -DNDEBUG -flto

# Flags for the Debug build.
# -g adds debugging information.
# -Wall -Wextra -pedantic provides extensive warnings.
# -DDEBUG is a common preprocessor definition for debug builds.
CXXFLAGS_DEBUG = -g -Wall -Wextra -pedantic -DDEBUG

# --- Build Targets ---

# The default target, executed when you just run `make`.
# This will build the 'release' version.
all: release

# The 'release' target.
# It sets the specific compiler flags for release and builds the executable.
release: CXXFLAGS = $(CXXFLAGS_COMMON) $(CXXFLAGS_RELEASE)
release: $(TARGET)

# The 'debug' target.
# It sets the specific compiler flags for debug and builds the executable.
debug: CXXFLAGS = $(CXXFLAGS_COMMON) $(CXXFLAGS_DEBUG)
debug: $(TARGET)

# --- Rules ---

# Rule for linking all the object files into the final executable.
$(TARGET): $(OBJS)
	@echo "Linking executable: $@"
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

# Rule for compiling a .cpp source file into a .o object file.
# $< is the source file name.
# $@ is the target object file name.
# -c tells the compiler to produce an object file without linking.
# -MMD -MP generates dependency files (.d) that track header includes.
# This ensures that files are recompiled if a header they include changes.
%.o: %.cpp
	@echo "Compiling: $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@ -MMD -MP

# The 'clean' target to remove generated files.
# The leading '-' tells make to ignore errors if files don't exist.
clean:
	@echo "Cleaning up project files..."
	-rm -f $(TARGET) $(OBJS) $(OBJS:.o=.d)

# Include the generated dependency files.
# This is what makes the build system aware of header file changes.
-include $(OBJS:.o=.d)

# --- Phony Targets ---
# Declares targets that are not actual files.
.PHONY: all release debug clean
