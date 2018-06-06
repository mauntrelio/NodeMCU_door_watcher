#include "AsyncHTTPClient.h"

static AsyncClient * aClient = NULL;

// async HTTP client
void sendHTTP(String requestBody, char * responseBody, const char * httpHost)
{

  if(aClient)//client already exists
    return;

  aClient = new AsyncClient();
  aClient->onError([](void * arg, AsyncClient * client, int error){
    Serial.println("Connection Error");
    aClient = NULL;
    delete client;
  }, NULL);

  aClient->onConnect([requestBody, responseBody](void * arg, AsyncClient * client){

    Serial.println("Connected!");

    //send the request
    Serial.println("Sending request:");
    Serial.println(requestBody);
    client->write(requestBody.c_str());
    Serial.println("Request sent");

    aClient->onError(NULL, NULL);

    client->onDisconnect([](void * arg, AsyncClient * c){
      Serial.println("Disconnected!");
      aClient = NULL;
      delete c;
    }, NULL);

    client->onData([responseBody](void * arg, AsyncClient * c, void * data, size_t len){
      String response = "";
      String body = "";
      Serial.print("\r\nReceived data: ");
      Serial.println(len);
      uint8_t * d = (uint8_t*)data;
      response = (char*) d;
      body = response.substring(response.indexOf("\r\n\r\n")+4);
      strcpy(responseBody, const_cast<char*>(body.c_str()));
      //body.toCharArray(responseBody, body.length());
      Serial.println(responseBody);
    }, NULL);

  }, NULL);

  if(!aClient->connect(httpHost, 80)){
    Serial.println("Connection Failed!");
    AsyncClient * client = aClient;
    aClient = NULL;
    delete client;
  }

}
