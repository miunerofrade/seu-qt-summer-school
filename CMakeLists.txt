cmake_minimum_required(VERSION 3.16)

project(untitled VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT_SYNC_DATA_DIR "${CMAKE_SOURCE_DIR}/data" CACHE PATH "Directory for runtime JSON data")

if(WIN32 AND NOT CMAKE_PREFIX_PATH AND EXISTS "D:/Qt/Qt/6.11.2/mingw_64")
    set(CMAKE_PREFIX_PATH "D:/Qt/Qt/6.11.2/mingw_64" CACHE PATH "Qt installation prefix")
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network LinguistTools Svg)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network LinguistTools Svg)

set(TS_FILES untitled_zh_CN.ts)

set(PROJECT_SOURCES
    # 应用程序
    src/app/applicationcontroller.cpp
    src/app/applicationcontroller.h
    src/app/mainwindow.cpp
    src/app/mainwindow.h
    src/app/mainwindow.ui
    src/main.cpp

    # 共享数据和控件
    src/common/money.h
    src/common/result.h
    src/data/datastore.cpp
    src/data/datastore.h
    src/data/idatarepository.h
    src/data/jsonrepository.cpp
    src/data/jsonrepository.h
    src/domain/entities.cpp
    src/domain/entities.h
    src/widgets/avatarwidget.h
    src/widgets/dateedit.cpp
    src/widgets/dateedit.h
    src/widgets/dialogstyle.cpp
    src/widgets/dialogstyle.h
    src/widgets/flowlayout.cpp
    src/widgets/flowlayout.h

    # 功能模块
    src/features/admin/admincontroller.cpp
    src/features/admin/admincontroller.h
    src/features/admin/adminservice.cpp
    src/features/admin/adminservice.h
    src/features/admin/managementdialogs.cpp
    src/features/admin/managementdialogs.h
    src/features/auth/logindialog.cpp
    src/features/auth/logindialog.h
    src/features/booking/bookingcontroller.cpp
    src/features/booking/bookingcontroller.h
    src/features/booking/bookingservice.cpp
    src/features/booking/bookingservice.h
    src/features/orders/ordercontroller.cpp
    src/features/orders/ordercontroller.h
    src/features/orders/orderdelegates.cpp
    src/features/orders/orderdelegates.h
    src/features/orders/orderfilterproxymodel.cpp
    src/features/orders/orderfilterproxymodel.h
    src/features/orders/orderservice.cpp
    src/features/orders/orderservice.h
    src/features/orders/ordertablemodel.cpp
    src/features/orders/ordertablemodel.h
    src/features/orders/refundservice.cpp
    src/features/orders/refundservice.h
    src/features/passengers/passengercontroller.cpp
    src/features/passengers/passengercontroller.h
    src/features/passengers/passengerfilterproxymodel.cpp
    src/features/passengers/passengerfilterproxymodel.h
    src/features/passengers/passengerservice.cpp
    src/features/passengers/passengerservice.h
    src/features/passengers/passengertablemodel.cpp
    src/features/passengers/passengertablemodel.h
    src/features/query/querycontroller.cpp
    src/features/query/querycontroller.h
    src/features/query/queryservice.cpp
    src/features/query/queryservice.h
    src/features/query/railwayqueryservice.cpp
    src/features/query/railwayqueryservice.h
    src/features/query/seattypecombodelegate.cpp
    src/features/query/seattypecombodelegate.h
    src/features/query/trainqueryfilterproxymodel.cpp
    src/features/query/trainqueryfilterproxymodel.h
    src/features/query/trainquerymodel.cpp
    src/features/query/trainquerymodel.h
    src/features/settings/settingscontroller.cpp
    src/features/settings/settingscontroller.h
    src/features/statistics/seatsharechartwidget.cpp
    src/features/statistics/seatsharechartwidget.h
    src/features/statistics/statisticscontroller.cpp
    src/features/statistics/statisticscontroller.h
    src/features/statistics/statisticsmodel.cpp
    src/features/statistics/statisticsmodel.h
    src/features/statistics/statisticsservice.cpp
    src/features/statistics/statisticsservice.h
    src/features/statistics/trendchartwidget.cpp
    src/features/statistics/trendchartwidget.h

    # 资源
    ${TS_FILES}
    resources.qrc
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(untitled
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(untitled SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(untitled
            ${PROJECT_SOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

target_link_libraries(untitled PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Svg)
target_include_directories(untitled PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_definitions(untitled PRIVATE
    QT_SYNC_DATA_DIR="${QT_SYNC_DATA_DIR}"
)

# 自 Qt 6.1 起，iOS 版 Qt 会自动设置 MACOSX_BUNDLE_GUI_IDENTIFIER。
# 但如果面向 iOS 或 macOS 开发，仍应考虑手动设置明确且固定的包标识符。
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.untitled)
endif()
set_target_properties(untitled PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS untitled
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(untitled)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)
    set(AUTH_TEST_SOURCES ${PROJECT_SOURCES})
    list(REMOVE_ITEM AUTH_TEST_SOURCES src/main.cpp ${TS_FILES})
    add_executable(auth_tests tests/auth_tests.cpp ${AUTH_TEST_SOURCES})
    target_include_directories(auth_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(auth_tests PRIVATE Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Svg)
    add_test(NAME auth_tests COMMAND auth_tests)
    set_tests_properties(auth_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(phase1_tests
        tests/phase1_tests.cpp
        src/domain/entities.cpp
        src/domain/entities.h
        src/common/result.h
        src/data/datastore.cpp
        src/data/datastore.h
        src/data/idatarepository.h
        src/data/jsonrepository.cpp
        src/data/jsonrepository.h
    )
    target_include_directories(phase1_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(phase1_tests PRIVATE Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME phase1_tests COMMAND phase1_tests)

    add_executable(phase2_tests
        tests/phase2_tests.cpp
        src/domain/entities.cpp
        src/domain/entities.h
        src/common/result.h
        src/data/datastore.cpp
        src/data/datastore.h
        src/data/idatarepository.h
        src/data/jsonrepository.cpp
        src/data/jsonrepository.h
        src/features/passengers/passengerfilterproxymodel.cpp
        src/features/passengers/passengerfilterproxymodel.h
        src/features/passengers/passengertablemodel.cpp
        src/features/passengers/passengertablemodel.h
        src/features/admin/adminservice.cpp
        src/features/admin/adminservice.h
        src/features/passengers/passengerservice.cpp
        src/features/passengers/passengerservice.h
    )
    target_include_directories(phase2_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(phase2_tests PRIVATE Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME phase2_tests COMMAND phase2_tests)

    add_executable(phase3_tests
        tests/phase3_tests.cpp
        src/domain/entities.cpp
        src/domain/entities.h
        src/common/result.h
        src/data/datastore.cpp
        src/data/datastore.h
        src/data/idatarepository.h
        src/data/jsonrepository.cpp
        src/data/jsonrepository.h
        src/features/query/trainqueryfilterproxymodel.cpp
        src/features/query/trainqueryfilterproxymodel.h
        src/features/query/trainquerymodel.cpp
        src/features/query/trainquerymodel.h
        src/features/query/queryservice.cpp
        src/features/query/queryservice.h
        src/features/query/railwayqueryservice.cpp
        src/features/query/railwayqueryservice.h
    )
    target_include_directories(phase3_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(phase3_tests PRIVATE Qt${QT_VERSION_MAJOR}::Test Qt${QT_VERSION_MAJOR}::Network)
    add_test(NAME phase3_tests COMMAND phase3_tests)

    add_executable(phase4_tests
        tests/phase4_tests.cpp
        src/domain/entities.cpp
        src/domain/entities.h
        src/common/result.h
        src/data/datastore.cpp
        src/data/datastore.h
        src/data/idatarepository.h
        src/data/jsonrepository.cpp
        src/data/jsonrepository.h
        src/features/booking/bookingservice.cpp
        src/features/booking/bookingservice.h
    )
    target_include_directories(phase4_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(phase4_tests PRIVATE Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME phase4_tests COMMAND phase4_tests)

    add_executable(phase5_tests
        tests/phase5_tests.cpp
        src/domain/entities.cpp
        src/domain/entities.h
        src/common/result.h
        src/data/datastore.cpp
        src/data/datastore.h
        src/data/idatarepository.h
        src/data/jsonrepository.cpp
        src/data/jsonrepository.h
        src/features/booking/bookingservice.cpp
        src/features/booking/bookingservice.h
        src/features/orders/orderservice.cpp
        src/features/orders/orderservice.h
        src/features/orders/refundservice.cpp
        src/features/orders/refundservice.h
    )
    target_include_directories(phase5_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(phase5_tests PRIVATE Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME phase5_tests COMMAND phase5_tests)

    add_executable(phase6_tests
        tests/phase6_tests.cpp
        src/domain/entities.cpp
        src/domain/entities.h
        src/common/result.h
        src/data/datastore.cpp
        src/data/datastore.h
        src/data/idatarepository.h
        src/data/jsonrepository.cpp
        src/data/jsonrepository.h
        src/features/statistics/statisticsservice.cpp
        src/features/statistics/statisticsservice.h
    )
    target_include_directories(phase6_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(phase6_tests PRIVATE Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME phase6_tests COMMAND phase6_tests)
endif()
