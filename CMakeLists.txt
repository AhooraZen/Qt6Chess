cmake_minimum_required(VERSION 3.25)

project(Qt6Chess VERSION 1.0.1 LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)



find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "ccache enabled: ${CCACHE_PROGRAM}")
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Svg Multimedia)

qt_standard_project_setup()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

qt_add_executable(Qt6Chess
    src/main.cpp
)

qt_add_qml_module(Qt6Chess
    URI Qt6Chess.UI
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES
        qml/Main.qml
        qml/board/ChessBoard.qml
        qml/board/ArrowOverlay.qml
        qml/components/EvalBar.qml
        qml/components/MoveHistory.qml
        qml/components/EnginePanel.qml
        qml/components/ChessClockView.qml
        qml/dialogs/PromotionDialog.qml
        qml/dialogs/GameOverDialog.qml
    RESOURCES
        resources/pieces/cburnett/wP.svg
        resources/pieces/cburnett/wN.svg
        resources/pieces/cburnett/wB.svg
        resources/pieces/cburnett/wR.svg
        resources/pieces/cburnett/wQ.svg
        resources/pieces/cburnett/wK.svg
        resources/pieces/cburnett/bP.svg
        resources/pieces/cburnett/bN.svg
        resources/pieces/cburnett/bB.svg
        resources/pieces/cburnett/bR.svg
        resources/pieces/cburnett/bQ.svg
        resources/pieces/cburnett/bK.svg
        resources/sounds/move.wav
        resources/sounds/capture.wav
        resources/sounds/check.wav
        resources/sounds/castle.wav
        resources/sounds/end.wav
    SOURCES
        src/core/ChessBoardModel.h
        src/core/ChessBoardModel.cpp
        src/engine/UciController.h
        src/engine/UciController.cpp
        src/game/GameController.h
        src/game/GameController.cpp
        src/game/ChessClock.h
        src/game/ChessClock.cpp
        src/game/MoveHistoryModel.h
        src/game/MoveHistoryModel.cpp
        src/audio/SoundManager.h
        src/audio/SoundManager.cpp
        src/cli/TerminalChess.h
        src/cli/TerminalChess.cpp
)

target_include_directories(Qt6Chess PRIVATE
    src/core
    src/engine
    src/game
    src/audio
    src/cli
)

target_link_libraries(Qt6Chess PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Svg
    Qt6::Multimedia
)

target_precompile_headers(Qt6Chess PRIVATE
    src/core/chess.hpp
    <QObject>
    <QString>
    <vector>
    <string>
)

# Automated unit tests
enable_testing()
add_subdirectory(tests)

# Installation targets
install(TARGETS Qt6Chess
    RUNTIME DESTINATION bin
)
install(FILES resources/qt6chess.desktop
    DESTINATION share/applications
)
install(FILES resources/icons/qt6chess.svg
    DESTINATION share/icons/hicolor/scalable/apps
)

