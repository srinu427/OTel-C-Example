# OTel-C-Example

## Dependencies:
  1. cmake: To build dependencies
  2. cJSON: Json Serilization Library [https://github.com/DaveGamble/cJSON]
  3. libcurl: For HTTP post to OTLP endpoint
  
## Building Dependencies:
### cJSON:
  1. From Git Source
  
      ```
      mkdir build
      cd build
      cmake ..
      make
      ```
  2. HomeBrew
  
      ```
      brew install cjson
      ```
  3. APT
  
      ```
      apt install libcjson-dev
      ```
### libcurl:
  1. Yum
    
      ```
      yum install curl-devel
      ```
    
  2. HomeBrew
    
      ```
      brew install curl
      ```
    
  3. APT
    
      ```
      apt install libcurl4-openssl-dev
      ```
      
## Compiling:
 
  ```
  gcc otel-example.c <libcjson so/dynlib> <libcurl so.dynlib>
  ```
  
## Notes:
  By default it pushes data to 0.0.0.0 expecting a OTLP exporter running with HTTP port on 4318
  To Quickly run OTLP exporter on your local machine run `docker-compose up` in the root of the git repo files
