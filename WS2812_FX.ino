/*
  Скетч создан на основе FASTSPI2 EFFECTS EXAMPLES автора teldredge (www.funkboxing.com)
  А также вот этой статьи https://www.tweaking4all.com/hardware/arduino/adruino-led-strip-effects/#cylon
  Доработан, переведён и разбит на файлы 2017 AlexGyver
  Отправляем в монитор порта номер режима, он активируется
*/

#include "FastLED.h"          // библиотека для работы с лентой

#define LED_COUNT 30          // число светодиодов в кольце/ленте
#define LED_DT 12             // пин, куда подключен DIN ленты

#define BTN_CTRL 2            // пин для вкл/выкл режима управления
#define BTN_INC 3             // пин для +
#define BTN_DEC 4             // пин для -

// следующие 3 константы не менять! В коде есть захардкоженое место
#define BTN_CTRL_BIT 0x04     // бит для режима управления
#define BTN_INC_BIT 0x02      // бит для кнопки "следующий режим"
#define BTN_DEC_BIT 0x01      // бит для кнопки "предыдущий режим"

// константы состояния кнопок
#define BTN_PRESS 1           // только что нажали
#define BTN_HOLD 2            // удерживается
#define BTN_RELEASE 3         // только что отпустили
#define BTN_NOTPRESS 4        // не нажата

#define BTN_MAX_THRESHOLD 80  // количество миллисекунд, прошедших после смены состояния кнопок (во избежание дребезга)

int max_bright = 100;         // максимальная яркость (0 - 255)
int ledMode = 8;
/*
  Стартовый режим
  0 - все выключены
  1 - все включены
  3 - кольцевая радуга
  888 - демо-режим
*/

// цвета мячиков для режима
byte ballColors[3][3] = {
  {0xff, 0, 0},
  {0xff, 0xff, 0xff},
  {0   , 0   , 0xff}
};

// ---------------СЛУЖЕБНЫЕ ПЕРЕМЕННЫЕ-----------------
int BOTTOM_INDEX = 0;        // светодиод начала отсчёта -- корректируется start_led'ом
int TOP_INDEX = int(LED_COUNT / 2);
int EVENODD = LED_COUNT % 2;
struct CRGB leds[LED_COUNT];
int ledsX[LED_COUNT][3];     //-ARRAY FOR COPYING WHATS IN THE LED STRIP CURRENTLY (FOR CELL-AUTOMATA, MARCH, ETC)

int thisdelay = 20;          //-FX LOOPS DELAY VAR
int thisstep = 10;           //-FX LOOPS STEP VAR
int thishue = 0;             //-FX LOOPS HUE VAR
int thissat = 255;           //-FX LOOPS SATURATION VAR

int thisindex = 0;
int thisRED = 0;
int thisGRN = 0;
int thisBLU = 0;

int idex = 0;                //-LED INDEX (0 to LED_COUNT-1)
int ihue = 0;                //-HUE (0-255)
int ibright = 0;             //-BRIGHTNESS (0-255)
int isat = 0;                //-SATURATION (0-255)
int bouncedirection = 0;     //-SWITCH FOR COLOR BOUNCE (0-1)
float tcount = 0.0;          //-INC VAR FOR SIN LOOPS
int lcount = 0;              //-ANOTHER COUNTING VAR

// свойские
int start_led = 0;                 // индекс светодиода, с которого рисуются эффекты (отличен от нуля в режиме управления)
int btns_state = 0;                // текущее состояние кнопок
int old_btns_state = 0;            // предыдущее состояние кнопок
unsigned long btn_cur_threshold[5] = {0}; // массив времени удержания кнопок
unsigned long btn_pressed[5] = {0};       // массив подсчёта отсчётов при удержании кнопок
unsigned long btn_released[5] = {0};      //   ... и при неудержании
int setup_mode = 0;                // флаг режима 0 -- норм, 1 -- режим управления
int setup_val = 2;                 // активный режим
unsigned long delay_pos = 0;       // счётчик, содержит миллисекунды начала задержки
// ---------------СЛУЖЕБНЫЕ ПЕРЕМЕННЫЕ-----------------

void start_delay() {
  // старт отсчёта задержки
  delay_pos = millis();
}

bool delay_reached() {
  // функция-флаг, прошла ли установленная задержка?
  return millis() - delay_pos > thisdelay;
}

int read_buttons() {
  // чтение нажатий кнопок (каждой свой бит)
  // пока захардкожено, согласуется с константами вида BTN_xxxx_BIT
  // не менять!
  int res = digitalRead(BTN_CTRL) << 2;
  res |= digitalRead(BTN_INC) << 1;
  res |= digitalRead(BTN_DEC);
  return res;
}

int bit_to_num(int nbit) {
  // возвращает индекс в массиве согласно переданному биту
  // юзается для индексации в массивах btn_xxx
  // пока реализован только младший байт
  switch (nbit) {
    case 0x01: return 0;
    case 0x02: return 1;
    case 0x04: return 2;
    case 0x08: return 3;
    case 0x10: return 4;
    case 0x20: return 5;
    case 0x40: return 6;
    case 0x80: return 7;
  }
}

int btn_state(int btn_bit) {
  // текущее состояние кнопок
  int btns = read_buttons();
  // индекс в массивах btn_xxx из полученного бита
  int nbit = bit_to_num(btn_bit);
  // результат
  int res = -1;
  // если время замера состояния не считается, запускается отсчёт
  if (btn_cur_threshold[nbit] == 0) btn_cur_threshold[nbit] = millis();
  if (btns & btn_bit) {
    // активен сигнал с кнопки
    // кнопка условно нажата
    btn_pressed[nbit]++;
    if (millis() - btn_cur_threshold[nbit] > BTN_MAX_THRESHOLD) {
      // отсчёт закончен
      if (btns_state & btn_bit) {
        // в предыдущем интервале кнопка уже была нажата, т.е. она удерживается
        res = BTN_HOLD;
        Serial.print("button ");
        Serial.print(btn_bit);
        Serial.println(" HOLDED");
      } else {
        // если кнопка была нажата более двух третей отведённого времени,
        // она переходит в состояние "нажата"
        if (btn_pressed[nbit] / 2 > btn_released[nbit]) {
          // в предыдущем интервале кнопка не была нажата, т.е. она нажата только что
          res = BTN_PRESS;
          old_btns_state = btns_state;
          btns_state |= btn_bit;
          Serial.print("button ");
          Serial.print(nbit);
          Serial.println(" PRESSED");
        }
      }
      btn_pressed[nbit] = 0;
      btn_released[nbit] = 0;
      btn_cur_threshold[nbit] = 0;
    }
    // отсчёт продолжается
  } else {
    // сигнал с кнопки не активен
    // кнопка условно отпущена
    btn_released[nbit]++;
    if (millis() - btn_cur_threshold[nbit] > BTN_MAX_THRESHOLD) {
      // отсчёт закончен
      if (btns_state & btn_bit) {
        // если кнопка была отпущена более двух третей отведённого времени,
        // она переходит в состояние "отпущена"
        if (btn_released[nbit] / 2 > btn_pressed[nbit]) {
          old_btns_state = btns_state;
          btns_state &= ~btn_bit;
          // в предыдущем интервале кнопка была нажата, т.е. её отпустило
          res = BTN_RELEASE;
          Serial.print("button ");
          Serial.print(nbit);
          Serial.println(" RELEASED");
        } else {
          // дебаг
//          Serial.println(btn_released[nbit] / 2);
//          Serial.println(btn_pressed[nbit]);
//          Serial.println(~1);
        }
      } else {
        // в предыдущем интервале кнопка не была нажата, т.е. она и не нажималась
        res = BTN_NOTPRESS;
//        Serial.print("button ");
//        Serial.print(btn_bit);
//        Serial.println(" NOTPRESSED");
      }
      btn_pressed[nbit] = 0;
      btn_released[nbit] = 0;
      btn_cur_threshold[nbit] = 0;
    }
  }
  return res;
}

void setup()
{
  Serial.begin(9600);              // открыть порт для связи
  pinMode(BTN_CTRL, INPUT);
  pinMode(BTN_INC, INPUT);
  pinMode(BTN_DEC, INPUT);

  LEDS.setBrightness(max_bright);  // ограничить максимальную яркость

  LEDS.addLeds<WS2812B, LED_DT, GRB>(leds, LED_COUNT);  // настрйоки для нашей ленты (ленты на WS2811, WS2812, WS2812B)
  one_color_all(0, 0, 0);          // погасить все светодиоды
  LEDS.show();                     // отослать команду
  
}

void loop() {

  // что с кнопками?
    // Внимание! Циклы не трогать, а пользоваться с умом.
  if (btn_state(BTN_CTRL_BIT) == BTN_PRESS)
    if (setup_mode == 1) {
      // включен режим управления, выключаю
      setup_mode = 0;
      start_led = 0;
      // установить режим, выбранный в управлении (на будущее)
      change_mode(setup_val);
      Serial.println("exit control mode");
    } else {
      // режим управления неактивен, включаю
      setup_mode = 1;
      // резерзирование первых шести светодиодов для индикации
      start_led = 6;
      Serial.println("enter control mode");
    }
  if (setup_mode == 1) { // выбор режима работы только в режиме управления
    if (btn_state(BTN_INC_BIT) == BTN_PRESS) {
      // следующий режим
      setup_val++;
      // кручу только первые 13, т.к. остальные ещё не адаптировал
      if (setup_val > 13) setup_val = 2;
      // задать режим
      change_mode(setup_val);
      Serial.println(setup_val);
    }
    if (btn_state(BTN_DEC_BIT) == BTN_PRESS) {
      // предыдущий режим
      setup_val--;
      // режимы 0 и 1 -- зарезервированы (0 -- всё выкл, 1 -- всё вкл)
      if (setup_val < 2) setup_val = 13;
      // задать режим
      change_mode(setup_val);
      Serial.println(setup_val);
    }
  }

  
  if (Serial.available() > 0) {     // если что-то прислали
    ledMode = Serial.parseInt();    // парсим в тип данных int
    change_mode(ledMode);           // меняем режим через change_mode (там для каждого режима стоят цвета и задержки)
  }

  bool show_led = delay_reached();
  if (show_led) {
    // установленная задержка закончилась, обновляю массив
    
    switch (ledMode) {
      case 999: break;                           // пауза
      case  2: rainbow_fade(); break;            // плавная смена цветов всей ленты
      case  3: rainbow_loop(); break;            // крутящаяся радуга
      case  4: random_burst(); break;            // случайная смена цветов
      case  5: color_bounce(); break;            // бегающий светодиод
      case  6: color_bounceFADE(); break;        // бегающий паровозик светодиодов
      case  7: ems_lightsONE(); break;           // вращаются красный и синий
      case  8: ems_lightsALL(); break;           // вращается половина красных и половина синих
      case  9: flicker(); break;                 // случайный стробоскоп
      case 10: pulse_one_color_all(); break;     // пульсация одним цветом
      case 11: pulse_one_color_all_rev(); break; // пульсация со сменой цветов
      case 12: fade_vertical(); break;           // плавная смена яркости по вертикали (для кольца)
      case 13: rule30(); break;                  // безумие красных светодиодов
      case 14: random_march(); break;            // безумие случайных цветов
      case 15: rwb_march(); break;               // белый синий красный бегут по кругу (ПАТРИОТИЗМ!)
      case 16: radiation(); break;               // пульсирует значок радиации
      case 17: color_loop_vardelay(); break;     // красный светодиод бегает по кругу
      case 18: white_temps(); break;             // бело синий градиент (?)
      case 19: sin_bright_wave(); break;         // тоже хрень какая то
      case 20: pop_horizontal(); break;          // красные вспышки спускаются вниз
      case 21: quad_bright_curve(); break;       // полумесяц
      case 22: flame(); break;                   // эффект пламени
      case 23: rainbow_vertical(); break;        // радуга в вертикаьной плоскости (кольцо)
      case 24: pacman(); break;                  // пакман
      case 25: random_color_pop(); break;        // безумие случайных вспышек
      case 26: ems_lightsSTROBE(); break;        // полицейская мигалка
      case 27: rgb_propeller(); break;           // RGB пропеллер
      case 28: kitt(); break;                    // случайные вспышки красного в вертикаьной плоскости
      case 29: matrix(); break;                  // зелёненькие бегают по кругу случайно
      case 30: new_rainbow_loop(); break;        // крутая плавная вращающаяся радуга
      case 31: strip_march_ccw(); break;         // чёт сломалось
      case 32: strip_march_cw(); break;          // чёт сломалось
      case 33: colorWipe(0x00, 0xff, 0x00, thisdelay);
        colorWipe(0x00, 0x00, 0x00, thisdelay); break;                                // плавное заполнение цветом
      case 34: CylonBounce(0xff, 0, 0, 4, 10, thisdelay); break;                      // бегающие светодиоды
      case 35: Fire(55, 120, thisdelay); break;                                       // линейный огонь
      case 36: NewKITT(0xff, 0, 0, 8, 10, thisdelay); break;                          // беготня секторов круга (не работает)
      case 37: rainbowCycle(thisdelay); break;                                        // очень плавная вращающаяся радуга
      case 38: TwinkleRandom(20, thisdelay, 1); break;                                // случайные разноцветные включения (1 - танцуют все, 0 - случайный 1 диод)
      case 39: RunningLights(0xff, 0xff, 0x00, thisdelay); break;                     // бегущие огни
      case 40: Sparkle(0xff, 0xff, 0xff, thisdelay); break;                           // случайные вспышки белого цвета
      case 41: SnowSparkle(0x10, 0x10, 0x10, thisdelay, random(100, 1000)); break;    // случайные вспышки белого цвета на белом фоне
      case 42: theaterChase(0xff, 0, 0, thisdelay); break;                            // бегущие каждые 3 (ЧИСЛО СВЕТОДИОДОВ ДОЛЖНО БЫТЬ НЕЧЁТНОЕ)
      case 43: theaterChaseRainbow(thisdelay); break;                                 // бегущие каждые 3 радуга (ЧИСЛО СВЕТОДИОДОВ ДОЛЖНО БЫТЬ КРАТНО 3)
      case 44: Strobe(0xff, 0xff, 0xff, 10, thisdelay, 1000); break;                  // стробоскоп
  
      case 45: BouncingBalls(0xff, 0, 0, 3); break;                                   // прыгающие мячики
      case 46: BouncingColoredBalls(3, ballColors); break;                            // прыгающие мячики цветные
  
      case 888: demo_modeA(); break;             // длинное демо
      case 889: demo_modeB(); break;             // короткое демо
    }
  }

  if (setup_mode == 1) {
    // активен режим управления => первый светодиод - синий
    leds[0].setRGB(0, 0, 255);
    // остальные (в количестве до start_led) показывают номер текущего режима минус один (чтобы с единицы начинались)
    for (int i = 1; i < start_led; i++) if ((setup_val - 1) & (1 << (i-1))) leds[i].setRGB(255, 0, 0); else leds[i].setRGB(0, 0, 0);
  } 
  if (show_led) {
    // установленная задержка закончилась, отправляю массив в ленту
    LEDS.show();
    // старт отсчёта задержки
    start_delay();
  }

}

void change_mode(int newmode) {
  thissat = 255;
  switch (newmode) {
    case 0: one_color_all(0, 0, 0); LEDS.show(); break; //---ALL OFF
    case 1: one_color_all(255, 255, 255); LEDS.show(); break; //---ALL ON
    case 2: thisdelay = 20; break;                      //---STRIP RAINBOW FADE
    case 3: thisdelay = 20; thisstep = 10; break;       //---RAINBOW LOOP
    case 4: thisdelay = 20; break;                      //---RANDOM BURST
    case 5: thisdelay = 20; thishue = 0; break;         //---CYLON v1
    case 6: thisdelay = 40; thishue = 0; break;         //---CYLON v2
    case 7: thisdelay = 40; thishue = 0; break;         //---POLICE LIGHTS SINGLE
    case 8: thisdelay = 40; thishue = 0; break;         //---POLICE LIGHTS SOLID
    case 9: thishue = 160; thissat = 50; break;         //---STRIP FLICKER
    case 10: thisdelay = 15; thishue = 0; break;        //---PULSE COLOR BRIGHTNESS
    case 11: thisdelay = 15; thishue = 0; break;        //---PULSE COLOR SATURATION
    case 12: thisdelay = 60; thishue = 180; break;      //---VERTICAL SOMETHING
    case 13: thisdelay = 100; break;                    //---CELL AUTO - RULE 30 (RED)
    case 14: thisdelay = 40; break;                     //---MARCH RANDOM COLORS
    case 15: thisdelay = 80; break;                     //---MARCH RWB COLORS
    case 16: thisdelay = 60; thishue = 95; break;       //---RADIATION SYMBOL
    //---PLACEHOLDER FOR COLOR LOOP VAR DELAY VARS
    case 19: thisdelay = 35; thishue = 180; break;      //---SIN WAVE BRIGHTNESS
    case 20: thisdelay = 100; thishue = 0; break;       //---POP LEFT/RIGHT
    case 21: thisdelay = 100; thishue = 180; break;     //---QUADRATIC BRIGHTNESS CURVE
    //---PLACEHOLDER FOR FLAME VARS
    case 23: thisdelay = 50; thisstep = 15; break;      //---VERITCAL RAINBOW
    case 24: thisdelay = 50; break;                     //---PACMAN
    case 25: thisdelay = 35; break;                     //---RANDOM COLOR POP
    case 26: thisdelay = 25; thishue = 0; break;        //---EMERGECNY STROBE
    case 27: thisdelay = 25; thishue = 0; break;        //---RGB PROPELLER
    case 28: thisdelay = 100; thishue = 0; break;       //---KITT
    case 29: thisdelay = 50; thishue = 95; break;       //---MATRIX RAIN
    case 30: thisdelay = 5; break;                      //---NEW RAINBOW LOOP
    case 31: thisdelay = 100; break;                    //---MARCH STRIP NOW CCW
    case 32: thisdelay = 100; break;                    //---MARCH STRIP NOW CCW
    case 33: thisdelay = 50; break;                     // colorWipe
    case 34: thisdelay = 50; break;                     // CylonBounce
    case 35: thisdelay = 15; break;                     // Fire
    case 36: thisdelay = 50; break;                     // NewKITT
    case 37: thisdelay = 20; break;                     // rainbowCycle
    case 38: thisdelay = 10; break;                     // rainbowTwinkle
    case 39: thisdelay = 50; break;                     // RunningLights
    case 40: thisdelay = 0; break;                      // Sparkle
    case 41: thisdelay = 20; break;                     // SnowSparkle
    case 42: thisdelay = 50; break;                     // theaterChase
    case 43: thisdelay = 50; break;                     // theaterChaseRainbow
    case 44: thisdelay = 100; break;                    // Strobe

    case 101: one_color_all(255, 0, 0); LEDS.show(); break; //---ALL RED
    case 102: one_color_all(0, 255, 0); LEDS.show(); break; //---ALL GREEN
    case 103: one_color_all(0, 0, 255); LEDS.show(); break; //---ALL BLUE
    case 104: one_color_all(255, 255, 0); LEDS.show(); break; //---ALL COLOR X
    case 105: one_color_all(0, 255, 255); LEDS.show(); break; //---ALL COLOR Y
    case 106: one_color_all(255, 0, 255); LEDS.show(); break; //---ALL COLOR Z
  }
  bouncedirection = 0;
  one_color_all(0, 0, 0);
  ledMode = newmode;
  start_delay();
}

