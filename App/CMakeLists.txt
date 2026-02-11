add_executable(ore ore.cpp)
target_link_libraries(ore PUBLIC ${OREA_LIB_NAME})
msvc_wpo_options(ore Release RelWithDebInfo)

install(TARGETS ore
    RUNTIME DESTINATION bin
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
