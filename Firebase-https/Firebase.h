#ifndef _FIREBASE_H_
#define _FIREBASE_H_

#include "mbed.h"
#include "NetworkInterface.h"
#include "https_request.h"

/* List of trusted root CA certificates
 * currently two: Amazon, the CA for os.mbed.com and Let's Encrypt,
 * the CA for httpbin.org
 * To add more root certificates, just concatenate them.
 */
const char SSL_CA_PEM[] =  
    "-----BEGIN CERTIFICATE-----\n"
    "MIIESjCCAzKgAwIBAgINAeO0mqGNiqmBJWlQuDANBgkqhkiG9w0BAQsFADBMMSAw\n"
    "HgYDVQQLExdHbG9iYWxTaWduIFJvb3QgQ0EgLSBSMjETMBEGA1UEChMKR2xvYmFs\n"
    "U2lnbjETMBEGA1UEAxMKR2xvYmFsU2lnbjAeFw0xNzA2MTUwMDAwNDJaFw0yMTEy\n"
    "MTUwMDAwNDJaMEIxCzAJBgNVBAYTAlVTMR4wHAYDVQQKExVHb29nbGUgVHJ1c3Qg\n"
    "U2VydmljZXMxEzARBgNVBAMTCkdUUyBDQSAxTzEwggEiMA0GCSqGSIb3DQEBAQUA\n"
    "A4IBDwAwggEKAoIBAQDQGM9F1IvN05zkQO9+tN1pIRvJzzyOTHW5DzEZhD2ePCnv\n"
    "UA0Qk28FgICfKqC9EksC4T2fWBYk/jCfC3R3VZMdS/dN4ZKCEPZRrAzDsiKUDzRr\n"
    "mBBJ5wudgzndIMYcLe/RGGFl5yODIKgjEv/SJH/UL+dEaltN11BmsK+eQmMF++Ac\n"
    "xGNhr59qM/9il71I2dN8FGfcddwuaej4bXhp0LcQBbjxMcI7JP0aM3T4I+DsaxmK\n"
    "FsbjzaTNC9uzpFlgOIg7rR25xoynUxv8vNmkq7zdPGHXkxWY7oG9j+JkRyBABk7X\n"
    "rJfoucBZEqFJJSPk7XA0LKW0Y3z5oz2D0c1tJKwHAgMBAAGjggEzMIIBLzAOBgNV\n"
    "HQ8BAf8EBAMCAYYwHQYDVR0lBBYwFAYIKwYBBQUHAwEGCCsGAQUFBwMCMBIGA1Ud\n"
    "EwEB/wQIMAYBAf8CAQAwHQYDVR0OBBYEFJjR+G4Q68+b7GCfGJAboOt9Cf0rMB8G\n"
    "A1UdIwQYMBaAFJviB1dnHB7AagbeWbSaLd/cGYYuMDUGCCsGAQUFBwEBBCkwJzAl\n"
    "BggrBgEFBQcwAYYZaHR0cDovL29jc3AucGtpLmdvb2cvZ3NyMjAyBgNVHR8EKzAp\n"
    "MCegJaAjhiFodHRwOi8vY3JsLnBraS5nb29nL2dzcjIvZ3NyMi5jcmwwPwYDVR0g\n"
    "BDgwNjA0BgZngQwBAgIwKjAoBggrBgEFBQcCARYcaHR0cHM6Ly9wa2kuZ29vZy9y\n"
    "ZXBvc2l0b3J5LzANBgkqhkiG9w0BAQsFAAOCAQEAGoA+Nnn78y6pRjd9XlQWNa7H\n"
    "TgiZ/r3RNGkmUmYHPQq6Scti9PEajvwRT2iWTHQr02fesqOqBY2ETUwgZQ+lltoN\n"
    "FvhsO9tvBCOIazpswWC9aJ9xju4tWDQH8NVU6YZZ/XteDSGU9YzJqPjY8q3MDxrz\n"
    "mqepBCf5o8mw/wJ4a2G6xzUr6Fb6T8McDO22PLRL6u3M4Tzs3A2M1j6bykJYi8wW\n"
    "IRdAvKLWZu/axBVbzYmqmwkm5zLSDW5nIAJbELCQCZwMH56t2Dvqofxs6BBcCFIZ\n"
    "USpxu6x6td0V7SvJCCosirSmIatj/9dSSVDQibet8q/7UK4v4ZUN80atnZz1yg==\n"
    "-----END CERTIFICATE-----\n";

/*
    // how to build the Firebase connection FirebaseUrl string for 'method overide'
    
    strcpy(FirebaseUrl, FirebaseID);                // Firebase account ID
    
    // this bit in the middle to send .json data structure and authority
    // for any REST api functions e.g. GET, PUT, POST, PATCH, DELETE
    // replace the PUT with required function
    
    strcat(FirebaseUrl, "/Parent/Child/.json?x-http-method-override=PUT&auth=");
    strcat(FirebaseUrl, FirebaseAuth);              // Firebase account authorisation key
*/

/*
// set Firebase project information..
char    FirebaseUrl[300];   // dimension to suit required character space
const char FirebaseID[100]  = "https://projectID.firebaseio.com";   // project ID 
const char FirebaseAuth[100]= "Web API key";                        // web API key
*/

// set Firebase project information..
char    FirebaseUrl[300];   // dimension to suit required character space
const char FirebaseID[100]  = "https://mbed-a5059.firebaseio.com";   // project ID 
const char FirebaseAuth[100]= "YPXm66X4m82sdy3srbPwcPYvpdNwrNUCAJ9xsZsL"; // web API key

char*   getData;
int     TLSfailText,httpfail;
/* Port number used to connect to the server */
const int server_port = 443;

#define IP         "192.168.1.180"
#define GATEWAY    "192.168.1.1"
#define NETMASK    "255.255.255.0" 

/*
  Connects to the network using the mbed_app.json ESP8266 WIFI networking interface,
  you can also swap this out with a driver for a different networking interface or
  if you use ETHERNET: change mbed_app.json "target.network-default-interface-type" : "ETHERNET",
*/
 
NetworkInterface*   net;
TLSSocket* socket = new TLSSocket();
SocketAddress Fbase;
nsapi_error_t result;
 
NetworkInterface *connect_to_default_network_interface() {
    printf("Connecting to network...\n\n");

    NetworkInterface* net = NetworkInterface::get_default_instance();
    
    // set static IP, not working on WIFI at the moment 
    //net->set_network((SocketAddress)IP,(SocketAddress)NETMASK,(SocketAddress)GATEWAY);
    
    if (!net) {
        printf("No network interface found, select an interface in 'mbed_app.json'\n");
        return NULL;
    }
    nsapi_error_t connect_status = net->connect();
    if (connect_status != NSAPI_ERROR_OK) {
        printf("Failed to connect to network (%d)\n", connect_status);
        return NULL;
    }
    SocketAddress net_addr;   
    net->get_ip_address(&net_addr);
    printf("IP address: %s\n", net_addr.get_ip_address() ? net_addr.get_ip_address() : "None");
    net->get_netmask(&net_addr);
    printf("Netmask:    %s\n", net_addr.get_ip_address() ? net_addr.get_ip_address() : "None");
    net->get_gateway(&net_addr);
    printf("Gateway:    %s\n", net_addr.get_ip_address() ? net_addr.get_ip_address() : "None");    
    printf("MAC:        %s\n", net->get_mac_address());  
    return net;
}
   
void startTLSreusesocket(char *FirebaseID) { 
          
    if ((result = socket->open(net)) != NSAPI_ERROR_OK) {
        printf("TLS socket open failed (%d)\n", result);
    }
    if ((result = socket->set_root_ca_cert(SSL_CA_PEM)) != NSAPI_ERROR_OK) {
        printf("TLS socket set_root_ca_cert failed (%d)\n", result);
    }   
    net->gethostbyname(FirebaseID, &Fbase);
    Fbase.set_port(server_port);
    if ((result = socket->connect(Fbase)) != NSAPI_ERROR_OK) {
        printf("Connect failure:\n%d\n", result);
        return;
    }
    printf("Successfully connected to:\n%s \nat port: %u\n",
                   FirebaseID, server_port);
}

void dump_response(HttpResponse* res) {
    mbedtls_printf("Status: %d - %s\n", res->get_status_code(), res->get_status_message().c_str()); 
    mbedtls_printf("Headers:\n");
    for (size_t ix = 0; ix < res->get_headers_length(); ix++) {
        mbedtls_printf("\t%s: %s\n", res->get_headers_fields()[ix]->c_str(), res->get_headers_values()[ix]->c_str());
    }
    mbedtls_printf("\nBody (%d bytes):\n\n%s\n", res->get_body_length(), res->get_body_as_string().c_str());
}

char *getFirebase(char* FirebaseUrl)
{       
    //HttpsRequest* get_req = new HttpsRequest(net, SSL_CA_PEM, HTTP_GET, FirebaseUrl);  // non socket reuse function
    HttpsRequest* get_req = new HttpsRequest(socket, HTTP_GET, FirebaseUrl);    // socket reuse function 
    HttpResponse* get_res = get_req->send();    
    if (!get_res) {       
        time_t seconds = time(NULL);                                        
        printf("Https GET failed (error code %d), %s", get_req->get_error(), ctime(&seconds));        
        socket->close();
        delete socket;  
        delete get_req;
        TLSSocket* socket = new TLSSocket();
        startTLSreusesocket((char*)FirebaseID); // restart TLS reuse socket if failure 
        return 0;}
        else{
            //dump_response(get_res);
            delete get_req;
            getData = (char*)get_res->get_body_as_string().c_str();                
            return getData;
            }
} 

bool putFirebase(char* FirebaseUrl, char *stringToProcess)
{
    //HttpsRequest* put_req = new HttpsRequest(net, SSL_CA_PEM, HTTP_PUT, FirebaseUrl);    // non socket reuse function
    HttpsRequest* put_req = new HttpsRequest(socket, HTTP_PUT, FirebaseUrl);    // socket reuse function   
    put_req->set_header("Content-Type", "application/json");
    HttpResponse* put_res = put_req->send(stringToProcess, strlen(stringToProcess));    
    if (!put_res) {
        time_t seconds = time(NULL);                                        
        printf("Https PUT failed (error code %d), %s", put_req->get_error(), ctime(&seconds));
        socket->close();
        delete socket;   
        delete put_req;
        TLSSocket* socket = new TLSSocket();                            
        startTLSreusesocket((char*)FirebaseID); // restart TLS reuse socket if failure 
        return 0;                
        }
        else{
            //dump_response(put_res);   
            delete put_req;     
            return put_res; 
            } 
}

bool postFirebase(char* FirebaseUrl, char *stringToProcess)
{  
   // HttpsRequest* post_req = new HttpsRequest(net, SSL_CA_PEM, HTTP_POST, FirebaseUrl);   // non socket reuse function
    HttpsRequest* post_req = new HttpsRequest(socket, HTTP_POST, FirebaseUrl);  // socket reuse function   
    post_req->set_header("Content-Type", "application/json");
    HttpResponse* post_res = post_req->send(stringToProcess, strlen(stringToProcess));    
    if (!post_res) {
        time_t seconds = time(NULL);                                        
        printf("Https POST failed (error code %d), %s", post_req->get_error(), ctime(&seconds));
        socket->close();
        delete socket;
        delete post_req;
        TLSSocket* socket = new TLSSocket();
        startTLSreusesocket((char*)FirebaseID); // restart TLS reuse socket if failure 
        return 0;
        }
        else{
            //dump_response(post_res);    
            delete post_req;     
            return post_res;
            }           
}

bool deleteFirebase(char* FirebaseUrl, char *stringToProcess)
{
    // HttpsRequest* delete_req = new HttpsRequest(net, SSL_CA_PEM, HTTP_DELETE, FirebaseUrl);   // non socket reuse function
    HttpsRequest* delete_req = new HttpsRequest(socket, HTTP_DELETE, FirebaseUrl);  // socket reuse function   
    delete_req->set_header("Content-Type", "application/json");
    HttpResponse* delete_res = delete_req->send(stringToProcess, strlen(stringToProcess));    
    if (!delete_res) {
        time_t seconds = time(NULL);                                        
        printf("Https DELETE failed (error code %d), %s", delete_req->get_error(), ctime(&seconds));
        socket->close();
        delete socket;
        delete delete_req;
        TLSSocket* socket = new TLSSocket();
        startTLSreusesocket((char*)FirebaseID); // restart TLS socket reuse if failure 
        return 0;
        }
        else{
            //dump_response(delete_res);    
            delete delete_req;     
            return delete_res;
            }    
}

#endif // _FIREBASE_H_
