#include "ui_strings.h"
#include "storage.h"

Language g_language = LANG_SERBIAN;  // Default language

const char* get_string(StringID id) {
  if (g_language == LANG_ENGLISH) {
    switch(id) {
      // Common
      case STR_READY:                    return "READY";
      case STR_MOTOR_ERROR:              return "MOTOR ERROR";
      case STR_HOMING_REQUIRED:          return "After alarm reset, homing is required.";
      case STR_MOVE_AWAY:                return "MOVE AWAY";
      case STR_MOVE_CLOSE:               return "MOVE CLOSE";
      case STR_POSITION:                 return "POSITION";
      case STR_TARGET:                   return "TARGET";
      case STR_MACHINE:                  return "MACHINE";
      case STR_MM:                       return "mm";
      case STR_GO_TO:                    return "GO TO";
      case STR_START:                    return "START";
      case STR_STOP:                     return "STOP";
      case STR_HOMING:                   return "HOMING";
      case STR_AUTO_RETRACT:             return "AUTO RETRACT";
      case STR_SAVE_SETTINGS:            return "SAVE SETTINGS";
      case STR_NAME:                     return "NAME:";
      case STR_THICKNESS:                return "THICKNESS:";
      case STR_OFFSET:                   return "OFFSET:";
      case STR_SELECT_MATERIAL:          return "SELECT MATERIAL";
      case STR_NOT_SELECTED:             return "— not selected —";
      case STR_STEP:                     return "STEP:";
      case STR_PARTS:                    return "PARTS:";
      case STR_SELECT_PROGRAM_START:     return "Select program and press START";
      case STR_MATERIAL:                 return "MAT:";
      case STR_ALARMS:                   return "ALARMS";
      case STR_ALARM_MESSAGE:            return "ALARM MESSAGE:";
      case STR_TEST_TOGGLE:              return "TEST TOGGLE:";
      case STR_TARGET_SET:               return "TARGET SET:";
      case STR_MOTOR_POS:                return "MOTOR:";
      case STR_PART_COUNT:               return "PARTS:";
      case STR_STEP_COUNT:               return "STEP:";
      case STR_STEP_POSITION:            return "POSITION:";
      
      // Settings
      case STR_SETTINGS:                 return "SETTINGS";
      case STR_LANGUAGE:                 return "LANGUAGE";
      case STR_LANGUAGE_SERBIAN:         return "Serbian";
      case STR_LANGUAGE_ENGLISH:         return "English";
      case STR_HOMING_FAST:              return "HOMING FAST";
      case STR_HOMING_SLOW:              return "HOMING SLOW";
      case STR_RETRACT_OFFSET:           return "RETRACT OFFSET";
      case STR_AUTO_RETRACT_SPEED:       return "AUTO RETRACT SPEED";
      case STR_STEP_SIZE:                return "STEP SIZE";
      case STR_POSITIONING:              return "POSITIONING";
      case STR_STEP_MM:                  return "STEPS/MM";
      
      // Programs & Materials
      case STR_PROGRAMS:                 return "PROGRAMS";
      case STR_MATERIALS:                return "MATERIALS";
      case STR_EDIT:                     return "EDIT";
      case STR_ADD:                      return "ADD";
      case STR_DELETE:                   return "DELETE";
      case STR_PROGRAM_NAME:             return "PROGRAM NAME:";
      case STR_MATERIAL_NAME:            return "MATERIAL NAME:";
      
      // Auto mode
      case STR_AUTO_MODE:                return "AUTO MODE";
      case STR_AUTO_WAITING_BEND:        return "Waiting for bend sensor...";
      case STR_AUTO_RETRACTING:          return "Retracting...";
      case STR_AUTO_PAUSED:              return "Paused";
      case STR_AUTO_COMPLETED:           return "Completed";
      
      default:                           return "?";
    }
  } else {
    // SERBIAN
    switch(id) {
      case STR_READY:                    return "SPREMAN";
      case STR_MOTOR_ERROR:              return "GRESKA MOTORA";
      case STR_HOMING_REQUIRED:          return "Nakon reseta alarma obavezno uraditi HOMING.";
      case STR_MOVE_AWAY:                return "UDALJI";
      case STR_MOVE_CLOSE:               return "PRIBLIZI";
      case STR_POSITION:                 return "POZICIJA";
      case STR_TARGET:                   return "TARGET";
      case STR_MACHINE:                  return "MACHINE";
      case STR_MM:                       return "mm";
      case STR_GO_TO:                    return "IDI NA";
      case STR_START:                    return "POKRENI";
      case STR_STOP:                     return "STOP";
      case STR_HOMING:                   return "HOMING";
      case STR_AUTO_RETRACT:             return "AUTO RETRACT";
      case STR_SAVE_SETTINGS:            return "SNIMI PODESAVANJA";
      case STR_NAME:                     return "NAZIV:";
      case STR_THICKNESS:                return "DEBLJINA:";
      case STR_OFFSET:                   return "OFFSET:";
      case STR_SELECT_MATERIAL:          return "IZABERI MATERIJAL";
      case STR_NOT_SELECTED:             return "— nije odabran —";
      case STR_STEP:                     return "KORAK:";
      case STR_PARTS:                    return "KOMADI:";
      case STR_SELECT_PROGRAM_START:     return "Odaberi program i pritisni START";
      case STR_MATERIAL:                 return "MAT:";
      case STR_ALARMS:                   return "ALARMI";
      case STR_ALARM_MESSAGE:            return "ALARM PORUKA:";
      case STR_TEST_TOGGLE:              return "TEST TOGGLE:";
      case STR_TARGET_SET:               return "ZADATA:";
      case STR_MOTOR_POS:                return "MOTOR:";
      case STR_PART_COUNT:               return "KOMADI:";
      case STR_STEP_COUNT:               return "KORAK:";
      case STR_STEP_POSITION:            return "POZICIJA:";
      
      // Settings
      case STR_SETTINGS:                 return "PODESAVANJA";
      case STR_LANGUAGE:                 return "JEZIK";
      case STR_LANGUAGE_SERBIAN:         return "Srpski";
      case STR_LANGUAGE_ENGLISH:         return "Engleski";
      case STR_HOMING_FAST:              return "HOMING BRZA";
      case STR_HOMING_SLOW:              return "HOMING SPORA";
      case STR_RETRACT_OFFSET:           return "RETRACT OFFSET";
      case STR_AUTO_RETRACT_SPEED:       return "AUTO RETRACT BRZINA";
      case STR_STEP_SIZE:                return "VELIČINA KORAKA";
      case STR_POSITIONING:              return "POZICIONIRANJE";
      case STR_STEP_MM:                  return "KORAKA/MM";
      
      // Programs & Materials
      case STR_PROGRAMS:                 return "PROGRAMI";
      case STR_MATERIALS:                return "MATERIJALI";
      case STR_EDIT:                     return "UREDI";
      case STR_ADD:                      return "DODAJ";
      case STR_DELETE:                   return "OBRIŠI";
      case STR_PROGRAM_NAME:             return "NAZIV PROGRAMA:";
      case STR_MATERIAL_NAME:            return "NAZIV MATERIJALA:";
      
      // Auto mode
      case STR_AUTO_MODE:                return "AUTO MOD";
      case STR_AUTO_WAITING_BEND:        return "Čeka bend senzor...";
      case STR_AUTO_RETRACTING:          return "Povlačenje...";
      case STR_AUTO_PAUSED:              return "Pauzirano";
      case STR_AUTO_COMPLETED:           return "Završeno";
      
      default:                           return "?";
    }
  }
}

void set_language(Language lang) {
  g_language = lang;
  storage_save_language();
}
