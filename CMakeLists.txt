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

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)

set(TS_FILES untitled_zh_CN.ts)

set(PROJECT_SOURCES
        src/main.cpp
        src/app/mainwindow.cpp
        src/app/mainwindow.h
        src/app/mainwindow.ui
        src/widgets/dateedit.cpp
        src/widgets/dateedit.h
        src/widgets/dialogstyle.cpp
        src/widgets/dialogstyle.h
        src/widgets/flowlayout.cpp
        src/widgets/flowlayout.h
        src/widgets/seatsharechartwidget.cpp
        src/widgets/seatsharechartwidget.h
        src/widgets/trendchartwidget.cpp
        src/widgets/trendchartwidget.h
        src/common/result.h
        src/domain/entities.cpp
        src/domain/entities.h
        src/data/datastore.cpp
        src/data/datastore.h
        src/data/idatarepository.h
        src/data/jsonrepository.cpp
        src/data/jsonrepository.h
        src/models/passengerfilterproxymodel.cpp
        src/models/passengerfilterproxymodel.h
        src/models/passengertablemodel.cpp
        src/models/passengertablemodel.h
        src/models/orderfilterproxymodel.cpp
        src/models/orderfilterproxymodel.h
        src/models/orderdelegates.cpp
        src/models/orderdelegates.h
        src/models/ordertablemodel.cpp
        src/models/ordertablemodel.h
        src/models/seattypecombodelegate.cpp
        src/models/seattypecombodelegate.h
        src/models/statisticsmodel.cpp
        src/models/statisticsmodel.h
        src/models/trainqueryfilterproxymodel.cpp
        src/models/trainqueryfilterproxymodel.h
        src/models/trainquerymodel.cpp
        src/models/trainquerymodel.h
        src/services/adminservice.cpp
        src/services/adminservice.h
        src/services/passengerservice.cpp
        src/services/passengerservice.h
        src/services/queryservice.cpp
        src/services/queryservice.h
        src/services/orderservice.cpp
        src/services/orderservice.h
        src/services/refundservice.cpp
        src/services/refundservice.h
        src/services/statisticsservice.cpp
        src/services/statisticsservice.h
        src/features/admin/admincontroller.cpp
        src/features/admin/admincontroller.h
        src/features/admin/managementdialogs.cpp
        src/features/admin/managementdialogs.h
        src/features/booking/bookingcontroller.cpp
        src/features/booking/bookingcontroller.h
        src/features/orders/ordercontroller.cpp
        src/features/orders/ordercontroller.h
        src/services/bookingservice.cpp
        src/services/bookingservice.h
        src/features/passengers/passengercontroller.cpp
        src/features/passengers/passengercontroller.h
        src/features/query/querycontroller.cpp
        src/features/query/querycontroller.h
        src/features/settings/settingscontroller.cpp
        src/features/settings/settingscontroller.h
        src/features/statistics/statisticscontroller.cpp
        src/features/statistics/statisticscontroller.h
        resources.qrc
        ${TS_FILES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(untitled
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET untitled APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(untitled SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(untitled
            ${PROJECT_SOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

target_link_libraries(untitled PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_include_directories(untitled PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_definitions(untitled PRIVATE
    QT_SYNC_DATA_DIR="${QT_SYNC_DATA_DIR}"
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
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
        src/models/passengerfilterproxymodel.cpp
        src/models/passengerfilterproxymodel.h
        src/models/passengertablemodel.cpp
        src/models/passengertablemodel.h
        src/services/adminservice.cpp
        src/services/adminservice.h
        src/services/passengerservice.cpp
        src/services/passengerservice.h
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
        src/models/trainqueryfilterproxymodel.cpp
        src/models/trainqueryfilterproxymodel.h
        src/models/trainquerymodel.cpp
        src/models/trainquerymodel.h
        src/services/queryservice.cpp
        src/services/queryservice.h
    )
    target_include_directories(phase3_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(phase3_tests PRIVATE Qt${QT_VERSION_MAJOR}::Test)
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
        src/services/bookingservice.cpp
        src/services/bookingservice.h
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
        src/services/bookingservice.cpp
        src/services/bookingservice.h
        src/services/orderservice.cpp
        src/services/orderservice.h
        src/services/refundservice.cpp
        src/services/refundservice.h
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
        src/services/statisticsservice.cpp
        src/services/statisticsservice.h
    )
    target_include_directories(phase6_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(phase6_tests PRIVATE Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME phase6_tests COMMAND phase6_tests)
endif()
