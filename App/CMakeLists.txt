add_executable(ore ore.cpp)
target_link_libraries(ore PUBLIC ${OREA_LIB_NAME} Boost::program_options)
msvc_wpo_options(ore Release RelWithDebInfo)

install(TARGETS ore
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
