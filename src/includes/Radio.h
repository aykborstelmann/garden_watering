#ifndef Radio_h
#define Radio_h

#include "RF24.h"
#include "RF24Network.h"
#include "RF24Mesh.h"
#include <SPI.h>
#include "Arduino.h"
#include <stdio.h>

class Radio {
    private:
        RF24 _rf24 = RF24(7, 8); 
        RF24Network _network = RF24Network(_rf24);
        RF24Mesh _mesh = RF24Mesh(_rf24,_network); 
        unsigned long _request_counter = 1;
        String _sensor_type;
        void (*_requestCallback)(request_payload, RF24NetworkHeader);
        void (*_responseCallback)(response_payload, RF24NetworkHeader);
        void (*_registrationCallback)(registration_payload, RF24NetworkHeader);
    public:
        void beginMesh(uint8_t nodeID) {
            _mesh.setNodeID(nodeID);
            if (nodeID != 0) {
                Serial.println(F("Connecting to the mesh..."));
            } else {
                Serial.println(F("Creating mesh ..."));
            }
            _mesh.begin();
        }
        void registerAtMaster(String sensor_type) {
            this->_sensor_type = sensor_type;
            registration_payload payload;
            payload.request_id = this->generateRequestID();
            payload.node_id = _mesh.getNodeID();
            sensor_type.toCharArray(payload.sensor_type,MAX_CHAR_SIZE);
            sendRegistration(payload);
        }
        void setRequestCallback(void (*requestCallback)(request_payload, RF24NetworkHeader)) {
            this->_requestCallback = requestCallback;
        }
        void setResponseCallback(void (*responseCallback)(response_payload, RF24NetworkHeader)) {
            this->_responseCallback = responseCallback;
        }
        void setRegistrationCallback(void (*registrationCallback)(registration_payload, RF24NetworkHeader)) {
            this->_registrationCallback = registrationCallback;
        }
        bool isMaster() {
            return _mesh.getNodeID() == 0;
        }
        RF24Mesh& getMesh() {
            return _mesh;
        }
        RF24Network& getNetwork() {
            return _network;
        }
        void update() {
            _mesh.update();

            if(this->isMaster()) {
                _mesh.DHCP();
            }

            this->checkConnection();
            
            while (this->packageAvailable()) {
                RF24NetworkHeader header = this->peekHeader();
                switch (header.type) {
                    case request_symbol:
                        _requestCallback(this->readRequest(),header);
                        break;
                    case response_symbol:
                        _responseCallback(this->readResponse(),header);
                        break;
                    case registration_symbol:
                        _registrationCallback(this->readRegistration(),header);
                        break;
                    default: 
                        _network.read(header,0,0);
                        break;
                }
            }

        }
        bool packageAvailable() {
            return _network.available();
        }
        RF24NetworkHeader peekHeader() {
            RF24NetworkHeader header; 
            _network.peek(header);
            return header;
        }
        // function for receiving requests
        request_payload readRequest() {
            RF24NetworkHeader header;
            request_payload payload;
            _network.read(header, &payload, sizeof(payload));
            return payload;
        }
        // function for sending requests
        bool sendRequest(request_payload& payload,uint16_t node) {
            if (!_mesh.write(&payload,request_symbol,sizeof(payload),node)) {
                this->checkConnection();
                return false;
            } else {
                Serial.print("Send ");
                printRequest(payload);
                return true;
            }
        }
        bool sendRequest(String attribute_requested,uint16_t node) {
            request_payload payload;
            payload.request_id = this->generateRequestID();
            String(attribute_requested).toCharArray(payload.attribute_requested,MAX_CHAR_SIZE);
            return sendRequest(payload, node);
        }
        // function for receiving responses
        response_payload readResponse() {
            RF24NetworkHeader header;
            response_payload payload;
            _network.read(header, &payload, sizeof(payload));
            return payload;
        }
        // function for sending responses
        bool sendResponse(response_payload& payload,uint16_t node) {
            if (!_mesh.write(&payload,response_symbol,sizeof(payload),node)) {
                this->checkConnection();
                return false;
            } else {
                Serial.print("Send ");
                printResponse(payload);
                return true;
            }
        }
        bool sendResponse(String value,request_payload& req_payload,uint16_t node) {
            response_payload payload;
            payload.request_id = req_payload.request_id;
            value.toCharArray(payload.value,MAX_CHAR_SIZE);
            return sendResponse(payload,node);
        }
        bool sendResponse(String value,registration_payload& reg_payload) {
            response_payload payload;
            payload.request_id = reg_payload.request_id;
            value.toCharArray(payload.value,MAX_CHAR_SIZE);
            return sendResponse(payload,reg_payload.node_id);
        }
        bool sendResponse(String value,request_payload& req_payload,RF24NetworkHeader& req_header) {
            return sendResponse(value,req_payload,_mesh.getNodeID(req_header.from_node));
        }
        // function for receiving responses
        registration_payload readRegistration() {
            RF24NetworkHeader header;
            registration_payload payload;
            _network.read(header, &payload, sizeof(payload));
            return payload;
        }
        // function for sending responses
        bool sendRegistration(registration_payload& payload) {
            if (!_mesh.write(&payload,registration_symbol,sizeof(payload),0)) {
                this->checkConnection();
                return false;
            } else {
                return true;
            }
        }
        unsigned long generateRequestID() {
            unsigned long request_id = 0;
            request_id += _mesh.getNodeID();
            request_id += (_request_counter * 100);
            _request_counter++;
            return request_id;
        };
        void checkConnection() {
            if (!this->isMaster()) {
                if (!_mesh.checkConnection()) {
                    Serial.println(F("Reconnecting ..."));
                    _mesh.renewAddress();
                    this->registerAtMaster(this->_sensor_type);
                }
            }
        }
        void printMesh() {
            Serial.println(" ");
            Serial.println(F("********Assigned Addresses********"));
            Serial.print("NodeID: ");
            Serial.print(_mesh.getNodeID());
            Serial.println(" RF24Network Address: 00");

            for(int i=0; i < _mesh.addrListTop; i++){
                Serial.print("NodeID: ");
                Serial.print(_mesh.addrList[i].nodeID);
                Serial.print(" RF24Network Address: 0");
                Serial.println(_mesh.addrList[i].address,OCT);
            }
            Serial.println(F("**********************************"));
            Serial.println(" ");
        }

};

#endif