Project(tri_tt_search)

FIND_PACKAGE(OpenCV REQUIRED)
INCLUDE_DIRECTORIES( ${OPENCV_INCLUDE_DIR})
LINK_LIBRARIES(${OpenCV_LIBS} )

ADD_EXECUTABLE(tri_tt_search tri_tt_search.cpp)

