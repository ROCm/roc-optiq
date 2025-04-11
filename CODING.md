# rocprofiler-visualizer Programming Guide
 This document provides information for how to develop rocprofiler-visualizer. Contibutors should ensure that they have read and understood the contents before attempting to submit code to the project.

## Project Structure
The rocprofiler-visualizer project is built from multiple components, including a number of open-source libraries.

- .: Project root.
    - CODING.md: This document.
    - CMakeLists.txt: Build file for the project.
    - README.md: Project read me and user documentation.
    - src: Source directory for all code developed specifically for rocprofiler-visualizer.
        - app: Application skeleton that provides the infrastructure for rocprofiler-visualizer.
        - .clang-format: Formatting for rocprofiler-visualizer code.
        - .clang-tidy: Formatting validation for rocprofiler-visualizer code.
    - thirdparty: Directory for all external libraries used by rocprofiler-visualizer.
        - glfw: GLFW provides the platform abstraction for window & event handling.
        - imgui: 'Dear ImGUI' is the user-interface toolkit used to implement rocprofiler-visualizer.
        - jsoncpp: 'JSON for Classic C++' is the JSON parsing library used in rocprofiler-visualizer.
        - ImGuiFileDialog: ImGuiFileDialog is a file selection dialog built for (and using only) Dear ImGui.
        - imgui-flame-graph: A Dear ImGui Widget for displaying Flame Graphs.
        - implot: ImPlot is an immediate mode, GPU accelerated plotting library for Dear ImGui.
        - CMakeLists.txt: Build instructions for thirdparty libraries.

## Project Requirements
The rocprofiler-visualizer project is designed to build & run on most desktop operating systems.

To build the project the following are required:
- A C++17 capable compiler & associated stdlib.
- Vulkan SDK: tested with 1.3.296.0 and later.
- CMake: tested with 3.31 and later.

## Build Instructions
- Install the Vulkan SDK.
- Install CMake.
- Run CMake to generate necessary build files.
- Build the project!

## Coding Style
The rocprofiler-visualizer project is part of the broader ROCm software stack and need to appear coherent with the broader SDK coding style. This section gives a brief rundown on the most common aspects of coding style that developers should be mindful of when contributing to rocprofiler-visualizer.

### General

- Each source line must not contain more than one statement.
- There should not be trailing whitespace other than a terminating new-line at the EOF (Git & IDEs complain about that).
- Adhere to C++17 and C11 standards respectively.
- Apply the K.I.S.S principle to code, especially for public APIs.
    - e.g. for a 'get_next_message()' function that must return an error **or** a message object, prefer a conventional solution over stdlib conveniences
        - Incorrect:
            std::optional<message> get_next_message();
        - Correct:
            error_t get_next_message(message& msg);
- Do not use 'goto', use formal flow control statements.
- All public APIs for a shared library should be written in C11, never expose C++ code (linkage is different between platforms & makes exposing a Foreign-Function-Interface harder).
- Do not throw C++ exceptions.
- Do not use memset/memcpy on non-P.O.D. types.
- Do not use operator overloading to change the meaning of the operator, only to implement standard functionality e.g. iterators.
- Return values from memory allocations must be checked for failure. The cleanest possible fallback path should be implemented.
- nullptr should be used instead of 0 or NULL when assigning or comparing null pointer values. nullptr provides additional type checking that results in cleaner code.
- Magic numbers must not be used. Replace with predefined constants with meaningful names.
- Assertions should be aggressively used to verify invariants and document the expected state of the program.
- Prefer compile-time assertions where possible.
- Assertions (and any other code wrapped in #if DEBUG) must not change the state of the program, only verify the current state.
- Avoid structure member packing - it is non-portable.
- Place braces on new lines to declare blocks.
- Do not use brace-less control flow statements - always use braces to enclose the block.
- Do not make variables public - declare an accessor for accessing variables.
- Use fixed-width integers.
- Prefer a single return statement in functions, but do not overcomplicate control-flow to achieve this.
- Assume the default is a positive result in control-flow statements,
    - e.g.:  if (result_ok == true) { // handle the positive case } else { // handle the error }
    - not: if (!result_ok) { return; }
    - Why do this? It is easier to read if success and failure conditions are separated like this.

### Memory Management

- Avoid heap allocations wherever possible, strongly prefer stack allocation.
- When a function must return a heap-allocated object prefer constructs that make ownership & lifetime of the allocation unambiguous.
    - e.g. std::unique_ptr, std::shared_ptr.
- If a function must return a raw pointer and the caller must take ownership ensure that the function name starts with 'alloc'.
    - e.g. my_type_t* t = alloc_my_type();
- Where there is a function that returns a raw pointer for which the caller is responsible provide a matching function whose name starts with 'free'.
    - e.g. free_my_type(t);

### C++ Specific

- Specify the storage class for all enums to allow forward declaration on all platforms.
- Strong enums (enum class) may be used as applicable.
- static_assert must be used for compile-time assertions.
- constexpr should be used to identify compile-time constant values.
- Do not use the 'auto' keyword, except:
    - In cases where specifying the type is either impossible or ugly enough to reduce readability, e.g., declaring iterators, pointers/references to anonymous structures.
    - In cases where the type is specified in the right-hand initialization expression.
- Minimize template usage other than for container types.
- Do not use templates that bloat code object size.

#### Namespaces

- Namespaces may be used to limit scope.
- The 'using' keyword must not be present in a header file.
- #include directives must not appear within the body of a namespace.
- Avoid using anonymous namespaces.
- There should be only one namespace hierarchy per file. If multiple namespace hierarchies are needed the file should be split.
- Use namespaces of the form 'namespace RocProfVis { namespace ModuleName {} }'.
- Within namespaces private struct/typedef/enum declarations that are not exported from a module may omit the 'rocprofvis_' prefix.

### Preprocessor

- Avoid preprocessor macros, prefer method or functions wherever possible.
- Do not retain blocks of code that are permanently disabled by a preprocessor directive - clearly document exceptions.
- Minimize blocks of code that are conditionally compiled depending on the value of a preprocessor macro.
- Constants or enumerations should be used instead of #defines.
- Do not use platform specific defines or pragmas unless absolutely required.
- If preprocessor macros and defines are needed, the macro and define names must be UPPERCASE and use underscores to separate words.

### Naming

| Construct                     | Format                                             | Example                       |
| ----------------------------- | -------------------------------------------------- | ----------------------------- |
| File Names                    | rocprofvis_ + module + lower_case_with_underscores | rocprofvis_controller.h       |
| Macros/Defines	            | ROCPROFVIS_ + UPPER_CASE_WITH_UNDERSCORES          | ROCPROFVIS_MY_CONST_VALUE     |
| Namespace Names	            | UpperCamelCase                                     | MyNamespace                   |
| Class Names                   | UpperCamelCase                                     | MyClass                       |
| Interface Class Names         | I + UpperCamelCase                                 | IMyInterface                  |
| Class Method Names	        | UpperCamelCase                                     | MyMemberFunction              |
| Static Class Member Names	    | s_ + lower_case_with_underscores                   | s_my_static_variable          |
| Non-static Class Member Names	| m_ + lower_case_with_underscores                   | m_my_member_variable          |
| Public Struct Names	        | rocprofvis_ + lower_case_with_underscores + _t     | rocprofvis_my_struct_type_t   |
| Struct Names in Namespaces	| lower_case_with_underscores + _t                   | my_struct_type_t              |
| Non-static Struct Member Names| lower_case_with_underscores                        | my_member_variable            |
| Public Typedefs/Aliases	    | rocprofvis_ + lower_case_with_underscores + _t     | rocprofvis_ + my_typedef_t    |
| Public Enumerations	        | rocprofvis_ + lower_case_with_underscores + _t     | rocprofvis_ + my_enum_t       |
| Typedefs/Aliases in Namespaces| lower_case_with_underscores + _t                   | my_typedef_t                  |
| Enum Class                  	| UpperCamelCase                                     | MyEnum                        |
| Enum Class Values	            | k + UpperCamelCase                                 | kMyEnumValue                  |
| Const Variables	            | UPPER_CASE_WITH_UNDERSCORES                        | MY_CONST_VALUE                |
| Global Variables	            | g_ + lower_case_with_underscores                   | g_my_global_variable          |
| TLS Variables	                | tls_ + lower_case_with_underscores                 | tls_my_tls_variable           |
| Non-const Variables	        | lower_case_with_underscores                        | my_non_const_variable         |
| Non-member Function Names	    | lower_case_with_underscores                        | my_free_function              |
| All function parameters	    | lower_case_with_underscores                        | my_function_parameter         |

### Files

- Source files must have an extension of ".c" or ".cpp".
- Include files must have an extension of ".h" or “.hpp”.
- Include files should use '#pragma once' to prevent multiple definitions.
- Do not include source files inside source files - no single-compilation-units.
- The header include order should be as follows:
    1. Matching .h/.hpp file (e.g. in foo.cpp, include foo.h first)
    2. System header files.
    3. Non-system external header files
    4. Local header files
    Headers within each group should be ordered alphabetically.
- In general, each class implementation should consist of at least one unique source file and at least one unique include file. More source and or include files may be used if the class implementation consists of a large number of source lines. Multiple classes may be contained in a single source and include file if these classes are small and the grouping is intuitive
- All headers must forward declare or include headers with declarations on which they depend. That is, every header must be able to be compiled when it is the only header included by a source file.
- Minimize header includes by forward declaring as much as possible.

### Documentation

- All files in the src/ directory should start with an AMD copyright & license header.
- Type declarations & function definitions should be properly documented so API documentation can be auto-generated.

