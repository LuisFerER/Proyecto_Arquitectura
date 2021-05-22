#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <SoftwareSerial.h> //**Nueva libreria**

//definimos los atributos para los diferentes modulos
#define RST_PIN         5          // Configurable, see typical pin layout above
#define SS_PIN          53         // Configurable, see typical pin layout above
#define I2C_ADDR    0x27
#define Gsm_tx 14 //**Definir el pin 18 como TX
#define Gsm_rx 15 //**Definir el pin 19 como RX

byte readCard[4]; // arreglo de 4 bytes para el ID de cada tarjeta
char* myTags[5] = {}; // arreglo para guardar hasta 5 tarjetas
int tagsIndex; //indice para verificar el espacio que se tiene disponible en el arreglo de tarjetas
String tagID = ""; // Id de cada tajeta en String
boolean successRead = false; // Indicador de lector de tarjeta
boolean correctTag = false; // Indicador de acceso de tarjeta
int proximitySensor = 0; // variable para el sensor de proximidad
boolean doorOpened = false; // variable de control de la puerta
String Numero_cliente = "4639526302"; //variable del numero al cual se estaran enviando los mensajes


// Creamos instancias
MFRC522 mfrc522(SS_PIN, RST_PIN); //instancia para el modulo MRFC (RFID)
LiquidCrystal_I2C lcd(I2C_ADDR,2, 1, 0, 4, 5, 6, 7); // instancia para la pantalla LCD
SoftwareSerial MOD_SIM800L(Gsm_tx, Gsm_rx); //**Nueva instancia**
Servo myServo; // instancia para el Servo motor

void setup() { 
  //inicializamos los modulos a utilizar
  SPI.begin();
  mfrc522.PCD_Init();
  lcd.begin (16,2);    // Inicializar el display con 16 caraceres 2 lineas
  lcd.setBacklightPin(3,POSITIVE);
  lcd.setBacklight(HIGH);
  pinMode(A2, INPUT);
  MOD_SIM800L.begin(115200); //Inicializar el modulo GSM SIM800L **
  
  //Primeramente tendremos que configurar la tarjeta maestra, para poder agregar y eliminar tarjetas.
  while(!successRead){
    lcd.setCursor(0,0);
    lcd.print("   Scan your");
    lcd.setCursor(0, 1);
    lcd.print(" The Master Tag!");
    successRead = obtenerID();
  }
  successRead = false;
  myTags[0] = strdup(tagID.c_str());
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(" The Master Tag");
  lcd.setCursor(0, 1);
  lcd.print("    Scanned!");
  delay(3000);

  //se inicializa el arreglo de tarjetas y mandamos un mensaje principal a la pantalla LCD.
  inicializarArreglo();
  mensajePrincipal();
  myServo.attach(8);  // Servo motor
  myServo.write(0);
}

void loop() {
  proximitySensor = analogRead(A0); //se lee el sensor CNY70 para obtener valores.
  if(proximitySensor > 300){ // Si la puerta esta cerrada...

    // CODIGO NUEVO
    if (digitalRead(A2) == 1) { // Comprobamos el estado del pulsador, si fue presionado hacer...
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(" Access Granted!");
      myServo.write(170); //Se abre el seguro de la puerta

      while(analogRead(A0) > 200);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("  Door Opened!");
      while(!doorOpened){
        proximitySensor = analogRead(A0);
        if(proximitySensor > 220){
          doorOpened = true;
        }
      }
      doorOpened = false;
      delay(500);
      myServo.write(10); // cerramos la puerta con seguro
      mensajePrincipal();
    } // FIN DEL NUEVO CODIGO
    
    if(obtenerID()){ // esperamos a que se escanee una tarjeta

      successRead = false;
      correctTag = false;
      //Comprueba si la tarjeta escaneada es la tarjeta maestra
      if(tagID == myTags[0]){
        lcd.clear();
        lcd.print("Program Mode:");
        lcd.setCursor(0, 1);
        lcd.print("Add/Remove Tag");
        while(!successRead){
          successRead = obtenerID();
          if(successRead == true){
            if(tagID == myTags[0]){ //Si la tarjeta escaneada es la tarjeta maestra, entonces...
              lcd.clear();
              lcd.setCursor(0,0);
              lcd.print(" The Master Tag");
              lcd.setCursor(0, 1);
              lcd.print(" Not Add/Remove");
              mensajePrincipal();
            }
            else { //de lo contrario, verificamos que haya coincidencia en nuestro arreglo
              for(int i = 1; i < 5; i++){ //for que empieza en 1 porque no se puede remover la tarjeta maestra
                if(tagID == myTags[i]){
                  myTags[i] = "";
                  lcd.clear();
                  lcd.setCursor(0,0);
                  lcd.print("  Tag Removed!");
                  mensajePrincipal();
                  return;
                }
              }
              //si no hubo coincidencia con alguna tarjeta ya agregada, verificamos si hay espacio para
              //agregar una nueva
              tagsIndex = espacioDisponible();
              if(tagsIndex != 0){ //Si el valor de tagsIndex es diferente que 0, entonces si hubo espacio.
                myTags[tagsIndex] = strdup(tagID.c_str());
                lcd.clear();
                lcd.setCursor(0,0);
                lcd.print("  Tag Added!");
                mensajePrincipal();
                return;
              }
              else{ //de lo contrario nuestro arreglo se encuentra saturado.
                lcd.clear();
                lcd.setCursor(0,0);
                lcd.print("    Tag Size");
                lcd.setCursor(0, 1);
                lcd.print("    Exceeded");
                mensajePrincipal();
                return;
              }
            } 
          }
        }//fin del while
      }// fin del if de la tarjeta maestra
      
      successRead = false;

      //Se comprueba si la tarjeta escaneada esta autorizada
      for(int i = 1; i < 5; i++){
        if(tagID == myTags[i]){
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print(" Access Granted!");
          myServo.write(170); //Se abre el seguro de la puerta
          Enviar_msj(Numero_cliente, "Se dio acceso a la persona con la tarjeta: " + tagID);
          delay(1000);
          
          /*Codigo nuevo*/
          while(analogRead(A0) > 300);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("  Door Opened!");
          while(!doorOpened){
            proximitySensor = analogRead(A0);
            if(proximitySensor > 300){
              doorOpened = true;
            }
          }
          doorOpened = false;
          delay(500);
          myServo.write(10); // cerramos la puerta con seguro
          /*Fin del nuevo codigo*/
          
          mensajePrincipal();
          correctTag = true;
        }
      }
      
      //Si no hubo coincidencia y la tarjeta escaneada no es la tarjeta maestra, entonces...
      if(correctTag  == false && tagID != myTags[0]){
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print(" Access Denied!");
        mensajePrincipal();
      }
    }//fin del metodo obtenerID()
  } //fin del if del sensor de proximidad
} //fin del loop

uint8_t obtenerID() {
  // Preparandose para leer tarjetas
  if ( ! mfrc522.PICC_IsNewCardPresent()) { //Si se coloca una nueva tarjeta en el lector RFID, continuar
    return false;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {   //Dado que una tarjeta se coloco en Serial, continuar
    return false;
  }
  tagID = "";
  for ( uint8_t i = 0; i < 4; i++) {  // Las tarjetas que utilizamos tienen un UID de 4 bytes.
    readCard[i] = mfrc522.uid.uidByte[i];
    tagID.concat(String(mfrc522.uid.uidByte[i], HEX)); // Agrega los 4 bytes en una sola variable String.
  }
  tagID.toUpperCase(); //La cadena se convierte en mayusculas
  mfrc522.PICC_HaltA(); // Se detiene la lectura
  return true;
}

void mensajePrincipal() {
  delay(1500);
  lcd.clear();
  lcd.print("-Access Control-");
  lcd.setCursor(0, 1);
  lcd.print(" Scan Your Tag!");
}

/**
 * Metodo para inicializar nuestro arreglo con espacios en blanco
 */
void inicializarArreglo(){
  for(int i = 1; i < 5; i++){
    myTags[i] = "";
  }
}

/**
 * Metodo que devuelve la posicion del arreglo en donde no se encuentra tarjeta para agregar una nueva.
 * En caso de que no haya espacios disponibles, la funcion regresa un 0, indicando que no hubo espacios
 * disponibles.
 */
int espacioDisponible(){
  int index = 0;
  for(int i = 1; i < 5; i++){
    if(myTags[i] == ""){
      index = i;
      break;
    }
  }
  return index;
}

void Enviar_msj(String numero, String msj) {
  //Se establece el formato de SMS en ASCII
  String config_numero = "AT+CMGS=\"+52" + numero + "\"\r\n";
  Serial.println(config_numero);

  //configurar modulo como modo SMS
  MOD_SIM800L.write("AT+CMGF=1\r\n");
  delay(1000);
  
  //Enviar comando para un nuevos SMS al numero establecido
  MOD_SIM800L.print(config_numero);
  delay(1000);

  //Enviar contenido del SMS
  MOD_SIM800L.print(msj);
  delay(1000);

  //Enviar Ctrl+Z
  MOD_SIM800L.write((char)26);
  delay(1000);
}
