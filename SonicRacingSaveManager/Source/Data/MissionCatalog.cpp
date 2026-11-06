// Generated from the game's files (DT_PlayMissionDisplayText: a Play Mission type's text and the parameters
// its label uses; Game.locres S2_DB_PlayMission playmission_text_*, DB_RaceData race_rank_*_text: en / es / fr / it)
#include "Data/MissionCatalog.h"

namespace SonicRacingSaveManager::Data::MissionCatalog {

const std::vector<MissionType>& All() {
    static const std::vector<MissionType> list = {
        { 1101001, 1, -1, { L"Race in any mode", L"Compite en cualquier modo", L"Fais la course dans n'importe quel mode", L"Gareggia in qualsiasi modalit\u00E0" } },
        { 1101002, 1, -1, { L"Race in Grand Prix", L"Compite en el Gran Premio", L"Fais la course dans un Grand Prix", L"Partecipa al Gran Premio" } },
        { 1101003, 1, -1, { L"Race in Race Park", L"Compite en el parque de competici\u00F3n", L"Fais la course dans l'Aire de comp\u00E9tition", L"Partecipa al Parco delle Corse" } },
        { 1101004, 1, -1, { L"Race in Time Trials", L"Compite en carreras contrarreloj", L"Fais la course en Contre la montre", L"Partecipa alla Corsa contro il Tempo" } },
        { 1101005, 1, -1, { L"Race in World Match", L"Compite en una partida global", L"Fais la course dans un Match mondial", L"Partecipa al World Match" } },
        { 1101006, 1, -1, { L"Race in Friend Match", L"Compite en una partida amistosa", L"Fais la course dans un Match amical", L"Partecipa alla gara amichevole" } },
        { 1101008, 1, -1, { L"Race in a Festival", L"Compite en un festival", L"Fais la course dans un Festival", L"Partecipa a un Festival" } },
        { 1101009, 1, -1, { L"Race in Legend Competition", L"Compite en una competici\u00F3n legendaria", L"Fais la course dans une Comp\u00E9tition des l\u00E9gendes", L"Partecipa alla Gara leggendaria" } },
        { 1101010, 1, -1, { L"Race in Rival Duel", L"Compite en un duelo con rival", L"Fais la course dans un Duel d'adversaires", L"Partecipa al Duello tra rivali" } },
        { 1101011, 1, -1, { L"Race in Duel Match", L"Compite en una partida a duelo", L"Fais la course dans un Duel", L"Partecipa al Duello" } },
        { 1102001, 2, 1, { L"Finish {Rank} or better in a Grand Prix race", L"Termina en {Rank} o superior en una carrera de un Gran Premio", L"Termine au moins \u00E0 la {Rank} place dans une course de Grand Prix", L"Piazzati almeno al {Rank} posto in una gara in modalit\u00E0 Gran Premio" } },
        { 1102002, 2, 1, { L"Finish {Rank} or better in a World Match race", L"Termina en {Rank} o superior en una carrera de una partida global", L"Termine au moins \u00E0 la {Rank} place dans une course de Match mondial", L"Piazzati almeno al {Rank} posto in una gara in modalit\u00E0 World Match" } },
        { 1102005, 2, 1, { L"Finish {Rank} or better in a Legend Competition race", L"Termina en {Rank} o superior en una carrera de una competici\u00F3n legendaria", L"Termine au moins \u00E0 la {Rank} place dans une course de Comp\u00E9tition des l\u00E9gendes", L"Piazzati almeno al {Rank} posto in una gara in modalit\u00E0 Gara leggendaria" } },
        { 1102007, 2, -1, { L"Finish 1st in Grand Prix", L"Termina en 1.\u00AA posici\u00F3n en el Gran Premio", L"Termine \u00E0 la 1re\u00A0place dans un Grand Prix", L"Piazzati al 1\u00B0 posto durante il Gran Premio" } },
        { 1102008, 2, -1, { L"Finish 1st in World Match", L"Termina en 1.\u00AA posici\u00F3n en una partida global", L"Termine \u00E0 la 1re\u00A0place dans un Match mondial", L"Piazzati al 1\u00B0 posto durante il World Match" } },
        { 1102011, 2, -1, { L"Finish 1st in Legend Competition", L"Termina en 1.\u00AA posici\u00F3n en una competici\u00F3n legendaria", L"Termine \u00E0 la 1re\u00A0place dans une Comp\u00E9tition des l\u00E9gendes", L"Piazzati al 1\u00B0 posto durante la Gara leggendaria" } },
        { 1102012, 2, -1, { L"Finish 1st as a team in a Festival", L"Termina en 1.\u00AA posici\u00F3n en equipo en un festival", L"Termine \u00E0 la 1re\u00A0place en \u00E9quipe dans un Festival", L"Piazzati al 1\u00B0 posto insieme alla tua squadra in un Festival" } },
        { 1102013, 2, -1, { L"Win a Duel Match", L"Gana una partida a duelo", L"Gagne un Duel", L"Vinci un Duello" } },
        { 1103001, 1, 1, { L"Get a {Progress}-win streak in Duel Match", L"Consigue una racha de {Progress} victorias en una partida a duelo", L"Remporte {Progress}\u00A0victoires cons\u00E9cutives dans un Duel", L"Ottieni una serie di {Progress} vittorie durante un Duello" } },
        { 1104001, 1, 1, { L"Get a {Progress}-win streak in a Festival", L"Consigue una racha de {Progress} victorias en un festival", L"Remporte {Progress}\u00A0victoires cons\u00E9cutives dans un Festival", L"Ottieni una serie di {Progress} vittorie durante un Festival" } },
        { 1105001, 1, -1, { L"Beat a Rival in Race Park", L"Derrota a un rival en el parque de competici\u00F3n", L"Bats un adversaire dans l'Aire de comp\u00E9tition", L"Sconfiggi un rivale nel Parco delle Corse" } },
        { 1105002, 1, -1, { L"Beat a Rival in World Match", L"Derrota a un rival en una partida global", L"Bats un adversaire dans un Match mondial", L"Sconfiggi un rivale durante il World Match" } },
        { 1106001, 1, -1, { L"Beat a Rival team in a Festival", L"Derrota a un equipo rival en un festival", L"Bats une \u00E9quipe adverse dans un Festival", L"Sconfiggi una squadra avversaria durante un Festival" } },
        { 1107001, 2, 1, { L"Beat a Rival level {Level} or higher in Rival Duel", L"Derrota a un rival de nivel {Level} o superior en un duelo", L"Bats un adversaire de niveau\u00A0{Level} minimum dans un Duel d'adversaires", L"Sconfiggi un rivale di livello {Level} o superiore in un Duello tra rivali" } },
        { 1107002, 2, 1, { L"Beat a Rival level {Level} or higher in Grand Prix", L"Derrota a un rival de nivel {Level} o superior en un Gran Premio", L"Bats un adversaire de niveau\u00A0{Level} minimum dans un Grand Prix", L"Sconfiggi un rivale di livello {Level} o superiore nel Gran Premio" } },
        { 1108001, 1, -1, { L"Get D Rank or higher in Time Trial", L"Consigue el rango D o superior en una carrera contrarreloj", L"Obtiens un rang D ou sup\u00E9rieur en Contre la montre", L"Ottieni il grado D o superiore in modalit\u00E0 Corsa contro il Tempo" } },
        { 1108002, 1, -1, { L"Get S Rank in Time Trial", L"Consigue el rango S en una carrera contrarreloj", L"Obtiens un rang S en Contre la montre", L"Ottieni il grado S in modalit\u00E0 Corsa contro il Tempo" } },
        { 1108003, 1, -1, { L"Get A Rank or higher in Time Trial", L"Consigue el rango A o superior en una carrera contrarreloj", L"Obtiens un rang A ou sup\u00E9rieur en Contre la montre", L"Ottieni il grado A o superiore in modalit\u00E0 Corsa contro il Tempo" } },
        { 1108004, 1, -1, { L"Get B Rank or higher in Time Trial", L"Consigue el rango B o superior en una carrera contrarreloj", L"Obtiens un rang B ou sup\u00E9rieur en Contre la montre", L"Ottieni il grado B o superiore in modalit\u00E0 Corsa contro il Tempo" } },
        { 1108005, 1, -1, { L"Get C Rank or higher in Time Trial", L"Consigue el rango C o superior en una carrera contrarreloj", L"Obtiens un rang C ou sup\u00E9rieur en Contre la montre", L"Ottieni il grado C o superiore in modalit\u00E0 Corsa contro il Tempo" } },
        { 1126001, 2, -1, { L"Race in World Match or Legend Competition", L"Compite en una partida global o competici\u00F3n legendaria", L"Fais la course dans un Match mondial ou dans une Comp\u00E9tition des l\u00E9gendes", L"Partecipa al World Match o alla Gara leggendaria" } },
        { 1127001, 0, -1, { L"Race online", L"Compite en l\u00EDnea", L"Fais la course en ligne", L"Gareggia online" } },
        { 1128001, 2, 1, { L"Finish {Rank} or better as a team in a Festival", L"Termina en {Rank} o superior en equipo en un festival", L"Termine au moins \u00E0 la {Rank} place en \u00E9quipe dans un Festival", L"Piazzati almeno al {Rank} posto insieme alla tua squadra in un Festival" } },
        { 1201001, 2, -1, { L"Race with a Speed Type Machine", L"Compite con un veh\u00EDculo de tipo velocidad", L"Fais la course avec une machine de type Vitesse", L"Gareggia con un veicolo di tipo velocit\u00E0" } },
        { 1201002, 2, -1, { L"Race with an Acceleration Type Machine", L"Compite con un veh\u00EDculo de tipo aceleraci\u00F3n", L"Fais la course avec une machine de type Acc\u00E9l\u00E9ration", L"Gareggia con un veicolo di tipo accelerazione" } },
        { 1201003, 2, -1, { L"Race with a Power Type Machine", L"Compite con un veh\u00EDculo de tipo potencia", L"Fais la course avec une machine de type Puissance", L"Gareggia con un veicolo di tipo potenza" } },
        { 1201004, 2, -1, { L"Race with a Handling Type Machine", L"Compite con un veh\u00EDculo de tipo conducci\u00F3n", L"Fais la course avec une machine de type Conduite", L"Gareggia con un veicolo di tipo stabilit\u00E0" } },
        { 1201005, 2, -1, { L"Race with a Boost Type Machine", L"Compite con un veh\u00EDculo de tipo turbo", L"Fais la course avec une machine de type Turbo", L"Gareggia con un veicolo di tipo turbo" } },
        { 1301001, 2, -1, { L"Race using a Ring Gain Gadget", L"Compite usando un artilugio Ganancia de Rings.", L"Fais la course en utilisant un gadget de gain de Rings", L"Gareggia usando un gadget Arraffa i Ring" } },
        { 1301002, 2, -1, { L"Race using a Ring Ability Gadget", L"Compite usando un artilugio Habilidades de Rings", L"Fais la course en utilisant un gadget de capacit\u00E9 de Rings", L"Gareggia usando il gadget Abilit\u00E0 Ring" } },
        { 1301003, 2, -1, { L"Race using a Technique Upgrade Gadget", L"Compite usando un artilugio Mejora de t\u00E9cnica", L"Fais la course en utilisant un gadget d'am\u00E9lioration technique", L"Gareggia usando il gadget Potenziamento tecniche" } },
        { 1301004, 2, -1, { L"Race using a Charge Upgrade Gadget", L"Compite usando un artilugio Mejora de carga", L"Fais la course en utilisant un gadget d'am\u00E9lioration de la charge", L"Gareggia usando il gadget Potenziamento carica" } },
        { 1301005, 2, -1, { L"Race using a Stat Upgrade Gadget", L"Compite usando un artilugio Mejora de par\u00E1metros", L"Fais la course en utilisant un gadget d'am\u00E9lioration des stats", L"Gareggia usando il gadget Potenziamento statistiche" } },
        { 1301006, 2, -1, { L"Race using a Support Gadget", L"Compite usando un artilugio Asistencia", L"Fais la course en utilisant un gadget de soutien", L"Gareggia usando il gadget Supporto" } },
        { 1301007, 2, -1, { L"Race using an Item Control Gadget", L"Compite usando un artilugio Control de objetos", L"Fais la course en utilisant un gadget de contr\u00F4le d'objets", L"Gareggia usando il gadget Controllo degli oggetti" } },
        { 1301008, 2, -1, { L"Race using an Upgrade/Add Item Gadget", L"Compite usando un artilugio Mejorar/A\u00F1adir objeto", L"Fais la course en utilisant un gadget d'am\u00E9lioration d'objets", L"Gareggia usando il gadget Potenzia/Aggiungi oggetto" } },
        { 1401001, 1, -1, { L"Race in a squad with another player", L"Compite en grupo con otro jugador", L"Fais la course en \u00E9quipe avec un autre joueur", L"Gareggia in una squadra con un altro giocatore" } },
        { 1401002, 1, -1, { L"Send a \"Nice!\" to another player and race", L"Env\u00EDa un \u00AB\u00A1Guay!\u00BB a otro jugador y compite.", L"Envoie un \"Bravo\u00A0!\" \u00E0 un autre joueur et fais la course", L"Invia un \"Grande!\" a un altro giocatore e gareggia" } },
        { 1401003, 1, -1, { L"Race with a decal equipped", L"Compite con una pegatina equipada", L"Fais la course en \u00E9quipant un autocollant", L"Gareggia con un adesivo" } },
    };
    return list;
}

const wchar_t* const kRanks[12][4] = {
        { L"1st", L"1.\u00BA", L"1er", L"1\u00B0" },
        { L"2nd", L"2.\u00BA", L"2e", L"2\u00B0" },
        { L"3rd", L"3.\u00BA", L"3e", L"3\u00B0" },
        { L"4th", L"4.\u00BA", L"4e", L"4\u00B0" },
        { L"5th", L"5.\u00BA", L"5e", L"5\u00B0" },
        { L"6th", L"6.\u00BA", L"6e", L"6\u00B0" },
        { L"7th", L"7.\u00BA", L"7e", L"7\u00B0" },
        { L"8th", L"8.\u00BA", L"8e", L"8\u00B0" },
        { L"9th", L"9.\u00BA", L"9e", L"9\u00B0" },
        { L"10th", L"10.\u00BA", L"10e", L"10\u00B0" },
        { L"11th", L"11.\u00BA", L"11e", L"11\u00B0" },
        { L"12th", L"12.\u00BA", L"12e", L"12\u00B0" },
};

} // namespace SonicRacingSaveManager::Data::MissionCatalog
