// CIS565 CUDA Rasterizer: A simple rasterization pipeline for Patrick Cozzi's CIS565: GPU Computing at the University of Pennsylvania
// Written by Yining Karl Li, Copyright (c) 2012 University of Pennsylvania

#include "main.h"

//-------------------------------
//-------------MAIN--------------
//-------------------------------

int main(int argc, char** argv){

  bool loadedScene = false;
  for(int i=1; i<argc; i++){
    string header; string data;
    istringstream liness(argv[i]);
    getline(liness, header, '='); getline(liness, data, '=');
    if(strcmp(header.c_str(), "mesh")==0){
      //renderScene = new scene(data);
      mesh = new obj();
      objLoader* loader = new objLoader(data, mesh); // objLoader class for encapsulation
      mesh->buildVBOs();
      delete loader;
      loadedScene = true;
    }
  }

  if(!loadedScene){
    cout << "Usage: mesh=[obj file]" << endl;
    return 0;
  }

  frame = 0;
  seconds = time (NULL);
  fpstracker = 0;

  projection = glm::perspective(fovy, float(width)/float(height), zNear, zFar);
  view = glm::lookAt(cameraPosition, lookatPosition, glm::vec3(0,1,0));
//  projection = projection * view;

  // Launch CUDA/GL
#ifdef __APPLE__
  // Needed in OSX to force use of OpenGL3.2 
  glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 3);
  glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 2);
  glfwOpenWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwOpenWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  init();
#else
  init(argc, argv);
#endif

  initCuda();

  initVAO();
  initTextures();

  GLuint passthroughProgram;
  passthroughProgram = initShader("shaders/passthroughVS.glsl", "shaders/passthroughFS.glsl");

  glUseProgram(passthroughProgram);
  glActiveTexture(GL_TEXTURE0);

#ifdef __APPLE__
    // send into GLFW main loop
    while(1){
      display();
      if (glfwGetKey(GLFW_KEY_ESC) == GLFW_PRESS || !glfwGetWindowParam( GLFW_OPENED )){
          kernelCleanup();
          cudaDeviceReset(); 
          exit(0);
      }
    }

    glfwTerminate();
#else
  glutDisplayFunc(display);
  glutKeyboardFunc(keyboard);
  glutMouseFunc(mouseClick);
  glutMotionFunc(mouseMotion);
  glutMouseWheelFunc(mouseWheel);


  glutMainLoop();
#endif
  kernelCleanup();
  return 0;
}

//-------------------------------
//---------RUNTIME STUFF---------
//-------------------------------

void runCuda(){
  // Map OpenGL buffer object for writing from CUDA on a single GPU
  // No data is moved (Win & Linux). When mapped to CUDA, OpenGL should not use this buffer
  dptr=NULL;

  vbo = mesh->getVBO();
  vbosize = mesh->getVBOsize();

  nbo = mesh->getNBO();
  nbosize = mesh->getNBOsize();

/*
  float newcbo[] = {0.0, 1.0, 0.0, 
                    0.0, 0.0, 1.0, 
                    1.0, 0.0, 0.0};*/

  float newcbo[] = {0.5, 0.5, 0.5, 
					0.5, 0.5, 0.5, 
					0.5, 0.5, 0.5};
  cbo = newcbo;
  cbosize = 9;

  ibo = mesh->getIBO();
  ibosize = mesh->getIBOsize();

  cudaGLMapBufferObject((void**)&dptr, pbo);
  cudaRasterizeCore(dptr, glm::vec2(width, height), frame, projection, view, zNear, zFar, lightPosition, vbo, vbosize, nbo, nbosize, cbo, cbosize, ibo, ibosize);
  cudaGLUnmapBufferObject(pbo);

  vbo = NULL;
  cbo = NULL;
  ibo = NULL;

  frame++;
  fpstracker++;

}

#ifdef __APPLE__

  void display(){
      runCuda();
      time_t seconds2 = time (NULL);

      if(seconds2-seconds >= 1){

        fps = fpstracker/(seconds2-seconds);
        fpstracker = 0;
        seconds = seconds2;

      }

      string title = "CIS565 Rasterizer | "+ utilityCore::convertIntToString((int)fps) + "FPS";

      glfwSetWindowTitle(title.c_str());


      glBindBuffer( GL_PIXEL_UNPACK_BUFFER, pbo);
      glBindTexture(GL_TEXTURE_2D, displayImage);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, 
            GL_RGBA, GL_UNSIGNED_BYTE, NULL);


      glClear(GL_COLOR_BUFFER_BIT);   

      // VAO, shader program, and texture already bound
      glDrawElements(GL_TRIANGLES, 6,  GL_UNSIGNED_SHORT, 0);

      glfwSwapBuffers();
  }

#else

  void display(){
    runCuda();
	time_t seconds2 = time (NULL);

    if(seconds2-seconds >= 1){

      fps = fpstracker/(seconds2-seconds);
      fpstracker = 0;
      seconds = seconds2;

    }

    string title = "CUDA Rasterizer | "+ utilityCore::convertIntToString((int)fps) + "FPS";
    glutSetWindowTitle(title.c_str());

    glBindBuffer( GL_PIXEL_UNPACK_BUFFER, pbo);
    glBindTexture(GL_TEXTURE_2D, displayImage);

	// Note: glTexSubImage2D will perform a format conversion if the
	// buffer is a different format from the texture. We created the
	// texture with format GL_RGBA8. In glTexSubImage2D we specified
	// GL_BGRA and GL_UNSIGNED_INT. This is a fast-path combination

	// Note: NULL indicates the data resides in device memory
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, 
        GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glClear(GL_COLOR_BUFFER_BIT);   

    // VAO, shader program, and texture already bound
    glDrawElements(GL_TRIANGLES, 6,  GL_UNSIGNED_SHORT, 0);

	glutSwapBuffers();
    glutPostRedisplay();
    
  }

  void keyboard(unsigned char key, int x, int y)
  {
    switch (key) 
    {
	   case(' '):
		   cameraPosition = glm::vec3(0,0,5);
		   lookatPosition = glm::vec3(0.0f);
		   viewPhi = 0.0f;
		   viewTheta = HALF_PI;
		   r = glm::length(cameraPosition);
		   view = glm::lookAt(cameraPosition, lookatPosition, glm::vec3(0,1,0));
		   printf("viewPhi = %f, viewTheta = %f\n", viewPhi * 180 / PI, viewTheta * 180 / PI);
		   utilityCore::printVec3(cameraPosition);
		   break;
       case(27):
           shut_down(1);    
           break;
    }
  }

  void mouseClick(int button, int state, int x, int y)
  {
	  if (state == GLUT_DOWN) {
		  button_mask |= 0x01 << button;
	  } 
	  else if (state == GLUT_UP) {
		  unsigned char mask_not = ~button_mask;
		  mask_not |= 0x01 << button;
		  button_mask = ~mask_not;
	  }

	  mouse_old_x = x;
	  mouse_old_y = y;
  }

  void mouseMotion(int x, int y)
  {
	  float dx, dy;
	  dx = (float)(x - mouse_old_x);
	  dy = (float)(y - mouse_old_y);

	  if (button_mask & 0x01) 
	  {// left button
		  viewPhi -= dx * 0.002f;
		  viewTheta -= dy * 0.002f;
		  viewTheta = glm::clamp(viewTheta, float(1e-6), float(PI-(1e-6)));
		  cameraPosition = glm::vec3(r*sin(viewTheta)*sin(viewPhi), r*cos(viewTheta) + lookatPosition.y, r*sin(viewTheta)*cos(viewPhi));
		  view = glm::lookAt(cameraPosition, lookatPosition, glm::vec3(0,1,0));
	  } 
	  if (button_mask & 0x02) 
	  {// middle button
		  cameraPosition.y += 0.02f*dy;
		  lookatPosition.y += 0.02f*dy;
		  view = glm::lookAt(cameraPosition, lookatPosition, glm::vec3(0,1,0));
	  }

	  mouse_old_x = x;
	  mouse_old_y = y;
  }

  void mouseWheel(int button, int dir, int x, int y)
  {
	  r -= dir>0 ? 0.1f : -0.1f;
	  r = glm::clamp(r, zNear, zFar);
	  cameraPosition = glm::vec3(r*sin(viewTheta)*sin(viewPhi), r*cos(viewTheta) + lookatPosition.y, r*sin(viewTheta)*cos(viewPhi));
	  view = glm::lookAt(cameraPosition, lookatPosition, glm::vec3(0,1,0));
  }
#endif
  
//-------------------------------
//----------SETUP STUFF----------
//-------------------------------

#ifdef __APPLE__
  void init(){

    if (glfwInit() != GL_TRUE){
      shut_down(1);      
    }

    // 16 bit color, no depth, alpha or stencil buffers, windowed
    if (glfwOpenWindow(width, height, 5, 6, 5, 0, 0, 0, GLFW_WINDOW) != GL_TRUE){
      shut_down(1);
    }

    // Set up vertex array object, texture stuff
    initVAO();
    initTextures();
  }
#else
  void init(int argc, char* argv[]){
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(width, height);
    glutCreateWindow("CIS565 Rasterizer");

    // Init GLEW
    glewInit();
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
      /* Problem: glewInit failed, something is seriously wrong. */
      std::cout << "glewInit failed, aborting." << std::endl;
      exit (1);
    }

    initVAO();
    initTextures();
  }
#endif

void initPBO(GLuint* pbo){
  if (pbo) {
    // set up vertex data parameter
    int num_texels = width*height;
    int num_values = num_texels * 4;
    int size_tex_data = sizeof(GLubyte) * num_values;
    
    // Generate a buffer ID called a PBO (Pixel Buffer Object)
    glGenBuffers(1,pbo);
    // Make this the current UNPACK buffer (OpenGL is state-based)
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, *pbo);
    // Allocate data for the buffer. 4-channel 8-bit image
    glBufferData(GL_PIXEL_UNPACK_BUFFER, size_tex_data, NULL, GL_DYNAMIC_COPY);
    cudaGLRegisterBufferObject( *pbo );
  }
}

void initCuda(){
  // Use device with highest Gflops/s
  cudaGLSetGLDevice( compat_getMaxGflopsDeviceId() );

  initPBO(&pbo);

  // Clean up on program exit
  atexit(cleanupCuda);

  runCuda();
}

void initTextures(){
    glGenTextures(1,&displayImage);
    glBindTexture(GL_TEXTURE_2D, displayImage);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA,
        GL_UNSIGNED_BYTE, NULL);
}

void initVAO(void){
    GLfloat vertices[] =
    { 
        -1.0f, -1.0f, 
         1.0f, -1.0f, 
         1.0f,  1.0f, 
        -1.0f,  1.0f, 
    };

    GLfloat texcoords[] = 
    { 
        1.0f, 1.0f,
        0.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f
    };

    GLushort indices[] = { 0, 1, 3, 3, 1, 2 };

    GLuint vertexBufferObjID[3];
    glGenBuffers(3, vertexBufferObjID);
    
    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObjID[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer((GLuint)positionLocation, 2, GL_FLOAT, GL_FALSE, 0, 0); 
    glEnableVertexAttribArray(positionLocation);

    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObjID[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(texcoords), texcoords, GL_STATIC_DRAW);
    glVertexAttribPointer((GLuint)texcoordsLocation, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(texcoordsLocation);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertexBufferObjID[2]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
}

GLuint initShader(const char *vertexShaderPath, const char *fragmentShaderPath){
    GLuint program = glslUtility::createProgram(vertexShaderPath, fragmentShaderPath, attributeLocations, 2);
    GLint location;

    glUseProgram(program);
    
    if ((location = glGetUniformLocation(program, "u_image")) != -1)
    {
        glUniform1i(location, 0);
    }

    return program;
}

//-------------------------------
//---------CLEANUP STUFF---------
//-------------------------------

void cleanupCuda(){
  if(pbo) deletePBO(&pbo);
  if(displayImage) deleteTexture(&displayImage);
}

void deletePBO(GLuint* pbo){
  if (pbo) {
    // unregister this buffer object with CUDA
    cudaGLUnregisterBufferObject(*pbo);
    
    glBindBuffer(GL_ARRAY_BUFFER, *pbo);
    glDeleteBuffers(1, pbo);
    
    *pbo = (GLuint)NULL;
  }
}

void deleteTexture(GLuint* tex){
    glDeleteTextures(1, tex);
    *tex = (GLuint)NULL;
}
 
void shut_down(int return_code){
  kernelCleanup();
  cudaDeviceReset();
  #ifdef __APPLE__
  glfwTerminate();
  #endif
//  getchar();
  exit(return_code);
}
