find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2 WebSockets Concurrent)
qt_standard_project_setup(REQUIRES 6.5)

set(QL_QML_FILES
    ../../qml/Main.qml
    ../../qml/Theme.qml
    ../../qml/Sidebar.qml
    ../../qml/TopBar.qml
    ../../qml/ChartPanel.qml
    ../../qml/ForecastCard.qml
    ../../qml/BacktestCard.qml
    ../../qml/Sparkline.qml
    ../../qml/Pill.qml)
set_source_files_properties(../../qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
foreach(f ${QL_QML_FILES})
    get_filename_component(n ${f} NAME)
    set_source_files_properties(${f} PROPERTIES QT_RESOURCE_ALIAS ${n})
endforeach()

file(GLOB QL_SAMPLES ${CMAKE_CURRENT_SOURCE_DIR}/../../data/sample/*.csv)
foreach(f ${QL_SAMPLES})
    get_filename_component(n ${f} NAME)
    set_source_files_properties(${f} PROPERTIES QT_RESOURCE_ALIAS data/${n})
endforeach()

qt_add_executable(QuantLens
    main.cpp AppController.cpp WatchlistModel.cpp ChartItem.cpp
    ../data/Asset.cpp ../data/MarketData.cpp ../data/LiveFeed.cpp)

qt_add_qml_module(QuantLens
    URI QuantLens
    VERSION 1.0
    QML_FILES ${QL_QML_FILES}
    RESOURCES ${QL_SAMPLES}
    SOURCES ChartItem.h)

target_include_directories(QuantLens PRIVATE .. ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(QuantLens PRIVATE ql_core
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::WebSockets Qt6::Concurrent)
