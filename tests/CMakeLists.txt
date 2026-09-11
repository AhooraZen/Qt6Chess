find_package(Qt6 6.8 REQUIRED COMPONENTS Test Core)

qt_add_executable(TestChessCore
    TestChessCore.cpp
    ../src/core/ChessBoardModel.cpp
    ../src/game/ChessClock.cpp
    ../src/game/MoveHistoryModel.cpp
)

target_include_directories(TestChessCore PRIVATE
    ../src/core
    ../src/game
)

target_link_libraries(TestChessCore PRIVATE
    Qt6::Core
    Qt6::Test
)

target_precompile_headers(TestChessCore PRIVATE
    ../src/core/chess.hpp
    <QObject>
    <QString>
)

add_test(NAME TestChessCore COMMAND TestChessCore)
