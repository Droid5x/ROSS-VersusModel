INCLUDE_DIRECTORIES(${ROSS_SOURCE_DIR} ${ROSS_BINARY_DIR})
## Must include binary dir for config.h

## If you will be using the ROSSIO library add this:
#INCLUDE_DIRECTORIES(${ROSSIO_SOURCE_DIR})

#Send files you have included in your model
SET(model_srcs
    myModel3_main.c
)

#Set the executable name here:
#You will be executing the ./myModel4 binary
#Main function (in this case) is founf in myModel3.c

ADD_EXECUTABLE(myModel3 myModel3_main.c ${model_srcs})


TARGET_LINK_LIBRARIES(myModel3 ROSS m)

