# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS main.c   buttonsource.c ssd1306.c ssd1306_i2c_new.c ssd1306_spi.c fingersource.c # list the source files of this component
    INCLUDE_DIRS        # optional, add here public include directories
    PRIV_INCLUDE_DIRS   # optional, add here private include directories
    REQUIRES       ssd1306 nvs_flash  # optional, list the public requirements (component names)
    PRIV_REQUIRES       # optional, list the private requirements
)
