#include "raylib.h"   // si raylib ang bida: window, drawing, mouse, keyboard, lahat yan
#include <stdio.h>    // para sa snprintf, yung gumagawa ng text na may kasamang numbers
#include <stdlib.h>   // para sa rand() at srand(), ang randomizer natin
#include <string.h>   // para sa strcpy, strcat, memset (pang-ayos ng text at memory)
#include <time.h>     // para sa time(NULL), gagamitin as seed para iba-iba ang laro every play
#include <stdarg.h>   // para sa va_list, kailangan ng addLine kasi pwede maraming arguments

#define SCREEN_W 1100 // lapad ng window (pixels)
#define SCREEN_H 720  // taas ng window (pixels)

#define MAX_HOSPITALS 30    // pinakamaraming hospital na kasya sa array (sobra-sobra pa, 10 lang gamit natin)
#define NAME_LEN 60         // max na haba ng pangalan ng hospital (kasama yung null terminator)
#define NUM_SERVICES 5      // ilang klase ng service meron (MH, CU, DE, LB, PH)
#define PATIENTS_PER_SHIFT 15 // ilang pasyente ang aasikasuhin mo sa isang shift
#define LOOKUPS_PER_SHIFT 2   // ilang beses ka lang pwede mag-directory lookup (limited, kaya wag sayangin)
#define START_TRUST 50        // panimulang trust mo, lowkey kalahati pa lang
#define TARGET_TRUST 85       // ito ang kailangan mong abutin para manalo, no cap

#define FREE_LOW_COST 0 // code para sa libre o mura na hospital
#define HIGH_COST 1     // code para sa mahal na hospital (private, usually)

/* ---------- Colors ---------- */
#define COL_BG      (Color){ 22, 27, 38, 255 }    // background ng buong app, dark navy vibes
#define COL_PANEL   (Color){ 36, 43, 58, 255 }    // kulay ng mga panel/card, medyo mas light kesa bg
#define COL_PANEL2  (Color){ 52, 61, 82, 255 }    // pangalawang panel color, gamit sa disabled button at empty bar
#define COL_HOVER   (Color){ 48, 58, 80, 255 }    // kulay ng row kapag naka-hover ang mouse
#define COL_TEXT    (Color){ 235, 238, 245, 255 } // main text color, off-white
#define COL_MUTED   (Color){ 150, 160, 180, 255 } // pang-secondary text, medyo pagod na gray
#define COL_DIM     (Color){ 95, 104, 124, 255 }  // pinaka-dim na gray, para sa disabled na text
#define COL_GREEN   (Color){ 76, 200, 120, 255 }  // green = good, tama, slay
#define COL_RED     (Color){ 235, 87, 87, 255 }   // red = mali, danger, urgent
#define COL_AMBER   (Color){ 240, 180, 70, 255 }  // amber = warning o highlight
#define COL_BLUE    (Color){ 70, 130, 220, 255 }  // blue = main button color

/* ---------- Game data ---------- */

const char *serviceList[NUM_SERVICES] = {   // array ng buong pangalan ng bawat service
    "Mental Health Support", "General Checkup", "Dental Checkup", // index 0, 1, 2
    "Laboratory", "Pharmacy"                                      // index 3, 4
};
const char *serviceShort[NUM_SERVICES] = { "MH", "CU", "DE", "LB", "PH" }; // short codes na lalabas sa chips (pang-cute at pang-tipid sa space)
const char *serviceLines[NUM_SERVICES] = {  // mga linya ng pasyente, nagsasabi kung ano ang kailangan nila
    "I've been feeling overwhelmed lately and I need someone to talk to.", // linya para sa Mental Health
    "I just need a general checkup. I haven't had one in a long time.",    // linya para sa Checkup
    "My tooth has been aching for days.",                                  // linya para sa Dental
    "My doctor asked me to get blood tests done.",                         // linya para sa Laboratory
    "I need to buy my maintenance medicine."                               // linya para sa Pharmacy
};

const char *patientNames[] = {  // listahan ng pwedeng pangalan ng pasyente
    "Aling Nena", "Mang Rodel", "Liza", "Jun", "Carmela", "Tatay Berting", // unang batch ng names
    "Grace", "Miguel", "Ate Joy", "Paolo", "Mariel", "Lolo Pido",          // pangalawang batch
    "Rhea", "Dennis", "Bea", "Mang Tonyo"                                  // pangatlong batch
};
#define NUM_NAMES ((int)(sizeof(patientNames) / sizeof(patientNames[0]))) // auto-count ng names: total size / size ng isa, kaya di mo na ilalagay by hand

typedef struct {                  // struct na naglalarawan sa isang hospital
    char name[NAME_LEN];          // pangalan ng hospital
    int offersService[NUM_SERVICES]; // 1 kung meron silang service, 0 kung wala (isang slot per service)
    int costCategory;             // FREE_LOW_COST o HIGH_COST
    int closed;                   // 1 kung sarado na, 0 kung bukas pa
} Hospital;                       // tawag natin dito ay "Hospital"

typedef struct {                  // struct na naglalarawan sa isang pasyente
    const char *name;             // pangalan ng pasyente (nakaturo lang sa patientNames)
    int needs[2];                 // hanggang dalawang service na kailangan niya
    int numNeeds;                 // ilan talaga ang kailangan: 1 o 2
    long income;                  // yearly income niya (PHP), long kasi malaki
    int urgent;                   // 1 kung urgent case (double penalty pag pumalpak)
} Patient;                        // tawag natin dito ay "Patient"

/* What happened after a referral, filled in by judgeReferral */
typedef struct {                  // struct ng resulta pagkatapos mo mag-refer
    char reaction[200];           // linya ng reaction ng pasyente (yung drama nila)
    char lines[3][100];           // hanggang 3 extra lines (bonus, streak, urgent, etc.)
    int nLines;                   // ilan na ang laman ng lines
    int change;                   // pagbabago sa trust (pwedeng + o -)
    int good;                     // 1 kung maganda ang referral, 0 kung pangit
} Outcome;                        // tawag natin dito ay "Outcome"

typedef enum { SCR_TITLE, SCR_NOTICE, SCR_PATIENT, SCR_RESULT, SCR_END } Screen; // lahat ng screens ng game, parang chapters

Hospital hospitals[MAX_HOSPITALS]; // ang mismong listahan ng mga hospital
int hospitalCount = 0;             // ilan na ang naka-load na hospital (nagsisimula sa zero)

/* mistake counters, used for the closing report */
int wrongService = 0, unaffordable = 0, overpay = 0, closedVisits = 0; // counters ng mga mali mo, ilalabas sa end report (walang judgment, promise)

/* ---------- Game state ---------- */
Screen screen = SCR_TITLE;     // anong screen ang nakikita ngayon, title muna
int trust = START_TRUST;       // totoong trust value
int shownTrust = START_TRUST;   /* animated copy of trust, for the meter */ // yung kita sa bar, dahan-dahang sumusunod sa trust para smooth
int streak = 0;                // ilang sunod-sunod na tamang referral
int lookups = LOOKUPS_PER_SHIFT; // ilan pang directory lookup ang natitira
int served = 0;                // ilang pasyente na ang natapos
int goodCount = 0;             // ilang good referral ang nagawa mo
int lastNoticeAt = -1;         // para di mag-repeat ang notice sa parehong patient number (-1 = wala pa)
Patient cur;                   // ang kasalukuyang pasyente (cur = current)
int lookupUsed = 0;            // 1 kung nag-lookup ka na sa pasyenteng 'to
int lookupMatches = 0;         // ilang hospital ang nag-match sa lookup
int highlight[MAX_HOSPITALS];  // 1 sa index ng hospital na dapat i-highlight
Outcome outcome;               // resulta ng huling referral
char noticeText[200];          // text ng closure notice
int endPage = 0;               // 0 = results page, 1 = "why this matters" page
int quitGame = 0;              // kapag naging 1, lalabas na ang game

/* ---------- Data ---------- */

void addHospital(const char *name, // function para magdagdag ng isang hospital sa listahan, name muna
                 int mental, int checkup, int dental, int lab, int pharmacy, // 1 o 0 kung meron ba yung service
                 int costCategory) { // libre/mura o mahal
    Hospital *h = &hospitals[hospitalCount]; // kunin ang susunod na empty slot sa array
    strcpy(h->name, name);                   // kopyahin ang pangalan papunta sa slot
    h->offersService[0] = mental;            // set kung may Mental Health
    h->offersService[1] = checkup;           // set kung may Checkup
    h->offersService[2] = dental;            // set kung may Dental
    h->offersService[3] = lab;               // set kung may Laboratory
    h->offersService[4] = pharmacy;          // set kung may Pharmacy
    h->costCategory = costCategory;          // i-save kung mura o mahal
    h->closed = 0;                           // bukas pa siya sa umpisa
    hospitalCount++;                         // dagdag na bilang, para next hospital sa susunod na slot na
}

void loadHospitals(void) { // nilo-load ang lahat ng hospital sa game, once lang 'to tinatawag sa main
    addHospital("Barangay Health Center - Bagumbayan",  1, 1, 0, 1, 1, FREE_LOW_COST); // walang dental, libre/mura
    addHospital("City General Hospital",                1, 1, 1, 1, 1, FREE_LOW_COST); // kumpleto lahat, libre/mura (jackpot)
    addHospital("St. Anne Medical Center (Private)",    1, 1, 1, 1, 1, HIGH_COST);     // kumpleto pero mahal
    addHospital("Barangay Health Center - Malinis",     0, 1, 0, 1, 1, FREE_LOW_COST); // walang MH at dental
    addHospital("Our Lady of Grace Hospital (Private)", 1, 1, 1, 1, 1, HIGH_COST);     // kumpleto, private, mahal
    addHospital("District Hospital - Riverside",        1, 1, 0, 1, 1, FREE_LOW_COST); // walang dental, mura
    addHospital("Riverside Medical Plaza (Private)",    0, 1, 1, 1, 1, HIGH_COST);     // walang MH, mahal
    addHospital("Community Health Center - Poblacion",  1, 1, 0, 1, 1, FREE_LOW_COST); // walang dental, mura
    addHospital("Poblacion Diagnostic & Dental Clinic", 0, 0, 1, 1, 0, HIGH_COST);     // dental at lab lang, mahal
    addHospital("Barangay Health Center - Maligaya",    0, 1, 0, 0, 1, FREE_LOW_COST); // checkup at pharmacy lang, mura
}

/* ---------- Income / referral rules (same as the terminal version) ---------- */

/* 0 = free/low only, 1 = both, 2 = high-cost only */
int getIncomeFilter(long income) {      // tinitingnan kung anong klaseng hospital ang bagay sa income
    if (income <= 330000) return 0;     // mababa ang income: libre/mura lang dapat
    if (income <= 1165000) return 1;    // middle: ok na ok kahit alin
    return 2;                           // mataas ang income: high-cost ang bagay
}

const char *incomeTierLabel(long income) {                     // kinukuha ang pangalan ng income tier para sa card
    if (income <= 130000) return "Poor";                       // pinakamababang tier
    if (income <= 330000) return "Low Income";                 // mababa
    if (income <= 665000) return "Lower Middle Income";        // lower middle
    if (income <= 1165000) return "Middle Income";             // middle
    if (income <= 1997000) return "Upper Middle Income";       // upper middle
    return "Rich";                                             // pinakamataas na tier
}

int costFits(int filter, int costCategory) {                        // tama ba ang presyo ng hospital sa filter?
    if (filter == 0) return costCategory == FREE_LOW_COST;          // filter 0: libre/mura lang ang pasado
    if (filter == 2) return costCategory == HIGH_COST;              // filter 2: high-cost lang ang pasado
    return 1;                                                       // filter 1: kahit ano pasado
}

int offersAllNeeds(const Hospital *h, const Patient *p) {  // meron ba ng LAHAT ng kailangan ng pasyente ang hospital?
    for (int i = 0; i < p->numNeeds; i++)                  // loop sa bawat need ng pasyente
        if (!h->offersService[p->needs[i]]) return 0;      // kapag may isang wala, fail agad (hindi pasok)
    return 1;                                              // lahat meron, pasok
}

/* A hospital that is a fully correct referral for this patient */
int isGoodReferral(const Hospital *h, const Patient *p) {   // tama ba talaga ang hospital na 'to para sa pasyente?
    return !h->closed && offersAllNeeds(h, p) &&            // bukas AT kumpleto ang services AND...
           costFits(getIncomeFilter(p->income), h->costCategory); // ...tama rin ang presyo. Tatlo dapat, walang kulang.
}

int hasSolution(const Patient *p) {                          // may kahit isang tamang hospital ba para sa pasyente?
    for (int i = 0; i < hospitalCount; i++)                  // check lahat ng hospital
        if (isGoodReferral(&hospitals[i], p)) return 1;      // may nakita, solved
    return 0;                                                // wala, impossible ang case na 'to
}

/* ---------- Game logic ---------- */

Patient makePatient(int patientNumber) {  // gumagawa ng random na pasyente, depende sa patient number
    Patient p;                            // pasyenteng bubuuin natin
    long lo[6] = { 30000, 130001, 330001, 665001, 1165001, 1997001 }; // pinakamababang income ng bawat tier
    long hi[6] = { 130000, 330000, 665000, 1165000, 1997000, 3500000 }; // pinakamataas na income ng bawat tier

    for (int tries = 0; tries < 1000; tries++) {            // subukan hanggang 1000x para makabuo ng valid na pasyente
        p.name = patientNames[rand() % NUM_NAMES];          // pumili ng random na pangalan
        p.needs[0] = rand() % NUM_SERVICES;                 // pumili ng random na unang service
        p.numNeeds = 1;                                     // default: isang need lang
        p.needs[1] = -1;                                    // -1 = walang pangalawang need

        /* from patient 4 on, half the patients need two services */
        if (patientNumber >= 4 && rand() % 2 == 0) {        // simula patient #4, 50% chance na dalawa ang need
            int second;                                     // dito ilalagay ang pangalawang service
            do { second = rand() % NUM_SERVICES; } while (second == p.needs[0]); // random ulit hanggang di kapareho ng una
            p.needs[1] = second;                            // i-save ang pangalawang need
            p.numNeeds = 2;                                 // dalawa na ang need niya
        }

        int tier = rand() % 6;                              // pumili ng random na income tier (0 hanggang 5)
        p.income = lo[tier] + (rand() % (int)((hi[tier] - lo[tier]) / 1000)) * 1000L; // random na income sa loob ng tier, naka-round sa libo
        p.urgent = (rand() % 5 == 0); /* about 1 in 5 */   // 1 sa 5 chance na urgent

        if (hasSolution(&p)) break; /* never hand out an impossible case */ // kung may tamang sagot, ok na, labas sa loop
    }
    return p;                                               // ibalik ang pasyente
}

int clampTrust(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); } // pinapanatili ang trust sa 0 hanggang 100, hindi lalampas

static void addLine(Outcome *o, const char *fmt, ...) { // nagdadagdag ng extra line sa outcome, parang printf ang gamit (static = sa file na 'to lang)
    if (o->nLines >= 3) return;                         // full na (3 lines max), skip na
    va_list ap;                                         // taga-hawak ng mga extra arguments
    va_start(ap, fmt);                                  // simulan ang pagbasa ng extra arguments
    vsnprintf(o->lines[o->nLines], sizeof o->lines[0], fmt, ap); // isulat ang formatted text sa susunod na slot, safe sa haba
    va_end(ap);                                         // tapusin ang pagbasa ng arguments
    o->nLines++;                                        // dagdag na bilang ng lines
}

/* Works out the trust change for a referral and fills in the outcome. */
void judgeReferral(const Patient *p, const Hospital *h, int *streakPtr, Outcome *o) { // ang utak ng game: hinuhusgahan ang referral mo (pointer sa streak para ma-update)
    int filter = getIncomeFilter(p->income);   // kunin ang income filter ng pasyente
    int change;                                // dito ilalagay ang trust change
    o->nLines = 0;                             // i-reset ang extra lines

    if (h->closed) {                           // CASE 1: sarado ang hospital
        snprintf(o->reaction, sizeof o->reaction, // gawin ang reaction text
                 "%s: \"I traveled all the way there and it was CLOSED!\"", p->name); // galit na pasyente, grabe
        closedVisits++;                        // dagdag sa counter ng closed visits
        change = -15;                          // minus 15 trust
    } else if (!offersAllNeeds(h, p)) {        // CASE 2: kulang ang services
        if (p->numNeeds == 2)                  // kung dalawa ang need...
            snprintf(o->reaction, sizeof o->reaction, // ...ito ang reaction
                     "%s: \"They could only help with part of what I needed...\"", p->name); // kalahati lang natulungan
        else                                   // kung isa lang ang need...
            snprintf(o->reaction, sizeof o->reaction, // ...ito naman ang reaction
                     "%s: \"They told me they don't even offer that here...\"", p->name); // wala talaga silang service na yun
        wrongService++;                        // dagdag sa counter ng maling service
        change = -15;                          // minus 15 trust
    } else if (filter == 0 && h->costCategory == HIGH_COST) { // CASE 3: mahirap ang pasyente pero mahal ang hospital
        snprintf(o->reaction, sizeof o->reaction, // gawin ang reaction text
                 "%s: \"I can't afford this place. I came all this way for nothing.\"", p->name); // di niya afford, sad
        unaffordable++;                        // dagdag sa counter ng unaffordable
        change = -20;                          // minus 20, pinakamabigat na penalty
    } else if (filter == 2 && h->costCategory == FREE_LOW_COST) { // CASE 4: mayaman ang pasyente pero libreng clinic ang pinuntahan
        snprintf(o->reaction, sizeof o->reaction, // gawin ang reaction text
                 "%s: \"It works... but it's packed and I wanted private care.\"", p->name); // pwede na pero gusto niya private
        overpay++;                             // dagdag sa counter (pangalan lang ng variable, hindi talaga overpay)
        change = -8;                           // minus 8, mild lang
    } else {                                   // CASE 5: TAMA lahat, slay
        change = (p->numNeeds == 2) ? 12 : 8;  // +12 kung dalawa ang need, +8 kung isa
        (*streakPtr)++;                        // dagdag sa streak (gamit ang pointer para mabago ang original)
        snprintf(o->reaction, sizeof o->reaction, // gawin ang reaction text
                 "%s: \"Thank you! This is exactly what I needed.\"", p->name); // masayang pasyente
        if (p->numNeeds == 2)                  // kung dalawa ang need...
            addLine(o, "Both needs handled in one visit: bonus points."); // ...may bonus message
        if (*streakPtr >= 3) {                 // kung 3 o higit na ang streak...
            int bonus = (*streakPtr - 2) * 2;  // ...compute ng bonus: +2 per extra streak
            if (bonus > 6) bonus = 6;          // max na bonus ay 6 lang
            addLine(o, "+%d streak bonus (%d good referrals in a row).", bonus, *streakPtr); // ipakita ang streak bonus
            change += bonus;                   // idagdag ang bonus sa change
        }
        if (p->urgent) {                       // kung urgent ang pasyente...
            addLine(o, "+4 for acting well on an urgent case."); // ...may extra message
            change += 4;                       // +4 bonus para sa urgent na naayos mo
        }
        o->change = change;                    // i-save ang final na change
        o->good = 1;                           // markahan na good ang referral
        return;                                // tapos na, di na dadaan sa penalty part sa baba
    }

    *streakPtr = 0;                            // kapag pumalpak, reset ang streak (ouch)
    if (p->urgent) {                           // kung urgent ang pasyente...
        addLine(o, "URGENT case: the penalty is doubled."); // ...sabihin na doble ang penalty
        change *= 2;                           // i-double ang negative na change
    }
    o->change = change;                        // i-save ang final na change
    o->good = 0;                               // markahan na hindi good ang referral
}

const char *rankTitle(int t, int survived) {                 // nagbibigay ng rank title base sa trust mo
    if (!survived) return "Dismissed";                       // kung na-fire ka (0 trust)
    if (t >= 95) return "Pillar of the Community";           // 95 pataas, legend status
    if (t >= TARGET_TRUST) return "Trusted Referral Officer"; // 85 pataas, panalo
    if (t >= 60) return "Reliable Staff";                    // 60 pataas, ok naman
    return "Needs More Training";                            // mas mababa pa, aral pa tayo
}

/* Close one random open hospital and prepare the notice text. */
void announceClosure(void) {                  // nagsasara ng random na hospital at gumagawa ng notice
    int idx, tries = 0;                       // idx = napiling hospital, tries = counter para di infinite loop
    do {                                      // ulitin hanggang makakuha ng bukas pa na hospital
        idx = rand() % hospitalCount;         // pumili ng random na hospital
        tries++;                              // dagdag ng subok
    } while (hospitals[idx].closed && tries < 100); // kung sarado na yung napili, ulit (max 100 tries)
    hospitals[idx].closed = 1;                // isara ang napili
    snprintf(noticeText, sizeof noticeText,   // gawin ang notice message
             "%s has CLOSED for the rest of the shift.", hospitals[idx].name); // sinasabi kung anong hospital ang nagsara
}

/* Moves the game to whatever screen comes next. */
void advance(void) {                          // taga-decide kung anong screen ang susunod
    if (trust <= 0 || served >= PATIENTS_PER_SHIFT) { // kung zero na ang trust o tapos na lahat ng pasyente...
        screen = SCR_END;                     // ...punta sa end screen
        endPage = 0;                          // simulan sa unang page ng end screen
        return;                               // tapos na ang function
    }
    /* surprise closures before patients 5, 10 and 13 */
    if ((served == 4 || served == 9 || served == 12) && lastNoticeAt != served) { // bago ang patient 5, 10, at 13 at di pa nakapag-notice dito
        announceClosure();                    // magsara ng random hospital
        lastNoticeAt = served;                // tandaan na may notice na dito para di maulit
        screen = SCR_NOTICE;                  // ipakita ang notice screen
        return;                               // labas na, notice muna
    }
    cur = makePatient(served + 1);            // gumawa ng bagong pasyente
    lookupUsed = 0;                           // reset: di pa nag-lookup sa bagong pasyente
    lookupMatches = 0;                        // reset ng match count
    memset(highlight, 0, sizeof highlight);   // alisin lahat ng highlight
    screen = SCR_PATIENT;                     // ipakita na ang patient screen
}

void resetGame(void) {                                           // nire-reset ang lahat para sa bagong shift
    for (int i = 0; i < hospitalCount; i++) hospitals[i].closed = 0; // buksan ulit lahat ng hospital
    wrongService = unaffordable = overpay = closedVisits = 0;    // i-zero ang lahat ng mistake counters
    trust = START_TRUST;                                         // balik sa starting trust
    shownTrust = START_TRUST;                                    // pati yung kitang bar, balik din
    streak = 0;                                                  // zero ang streak
    lookups = LOOKUPS_PER_SHIFT;                                 // ibalik ang lookups
    served = 0;                                                  // zero ang natapos na pasyente
    goodCount = 0;                                               // zero ang good referrals
    lastNoticeAt = -1;                                           // reset ng notice tracker
    endPage = 0;                                                 // balik sa unang page ng end screen
    advance();                                                   // simulan na, punta sa unang screen
}

void doLookup(void) {                        // ginagawa pag pinindot ang Directory lookup button
    if (lookups <= 0 || lookupUsed) return;  // kung ubos na lookups o nagamit na sa pasyenteng 'to, wag na
    lookups--;                               // bawas ng isang lookup
    lookupUsed = 1;                          // markahan na nagamit na
    lookupMatches = 0;                       // reset ng match count
    for (int i = 0; i < hospitalCount; i++) { // check lahat ng hospital
        highlight[i] = isGoodReferral(&hospitals[i], &cur); // i-highlight kung tamang referral
        if (highlight[i]) lookupMatches++;   // kung match, dagdag sa bilang
    }
}

void doReferral(int idx) {                                       // ginagawa pag nag-click ka ng hospital (idx = alin yung hospital)
    judgeReferral(&cur, &hospitals[idx], &streak, &outcome);     // hatulan ang referral mo
    if (outcome.change > 0) goodCount++;                         // kung positive ang change, dagdag sa good count
    trust = clampTrust(trust + outcome.change);                  // update ng trust, naka-limit sa 0 hanggang 100
    served++;                                                    // isa nang pasyente ang natapos
    screen = SCR_RESULT;                                         // ipakita ang result screen
}

/* ---------- Drawing helpers ---------- */

Color lighten(Color c) {                                                  // pinapaliwanag ang kulay, pang-hover effect
    Color r = c;                                                          // kopya ng original na kulay
    r.r = (unsigned char)(c.r + 30 > 255 ? 255 : c.r + 30);               // dagdag 30 sa red, hanggang 255 lang
    r.g = (unsigned char)(c.g + 30 > 255 ? 255 : c.g + 30);               // dagdag 30 sa green, hanggang 255 lang
    r.b = (unsigned char)(c.b + 30 > 255 ? 255 : c.b + 30);               // dagdag 30 sa blue, hanggang 255 lang
    return r;                                                             // ibalik ang mas maliwanag na kulay
}

/* Returns 1 on the frame the button is clicked. */
int drawButton(Rectangle r, const char *label, int fs, Color base, int enabled) { // dinadrawing ang button at chine-check kung na-click
    int hover = enabled && CheckCollisionPointRec(GetMousePosition(), r); // naka-hover ba ang mouse (kung enabled lang)
    Color c = !enabled ? COL_PANEL2 : (hover ? lighten(base) : base);    // disabled = gray, hover = lighter, normal = base color
    DrawRectangleRec(r, c);                                              // iguhit ang button
    DrawText(label,                                                      // iguhit ang text ng button...
             (int)(r.x + (r.width - MeasureText(label, fs)) / 2),        // ...naka-center pahalang
             (int)(r.y + (r.height - fs) / 2),                           // ...naka-center patayo
             fs, enabled ? WHITE : COL_DIM);                             // ...puti kung enabled, dim kung hindi
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);             // 1 lang kung naka-hover AT kakapindot lang ng left click
}

void drawCentered(const char *text, int cx, int y, int fs, Color c) {    // naglalagay ng text na naka-center sa isang x position
    DrawText(text, cx - MeasureText(text, fs) / 2, y, fs, c);            // x = center minus kalahati ng lapad ng text
}

/* Word-wrapped text. Returns the y position just below the last line. */
int drawWrapped(const char *text, int x, int y, int maxW, int fs, Color c) { // nagdadrawing ng mahabang text na pumapaling sa next line kapag lampas na sa maxW
    char line[1024] = "";                      // yung kasalukuyang linyang binubuo
    char word[128];                            // yung kasalukuyang salitang binabasa
    const char *p = text;                      // pointer na naglalakad sa text

    while (*p) {                               // habang may natitira pang text
        int n = 0;                             // haba ng salita
        while (*p == ' ') p++;                 // lampasan ang mga space
        while (*p && *p != ' ' && n < 127) word[n++] = *p++; // kopyahin ang salita hanggang space o dulo
        word[n] = 0;                           // lagyan ng null terminator para maging valid na string
        if (n == 0) break;                     // wala nang salita, tapos na

        char test[1200];                       // pansubok na linya (line + bagong salita)
        if (line[0]) snprintf(test, sizeof test, "%s %s", line, word); // kung may laman na ang line, idugtong ang salita
        else snprintf(test, sizeof test, "%s", word);                  // kung wala pa, yung salita lang

        if (MeasureText(test, fs) > maxW && line[0]) { // kung lampas na sa lapad at may laman ang line...
            DrawText(line, x, y, fs, c);       // ...iguhit muna ang kasalukuyang line
            y += fs + 6;                       // ...bumaba ng isang linya (may 6px gap)
            snprintf(line, sizeof line, "%s", word); // ...simulan ang bagong line gamit ang salitang di kasya
        } else {                               // kung kasya pa...
            snprintf(line, sizeof line, "%.1000s", test); // ...ilagay na sa line ang dinugtong na bersyon
        }
    }
    if (line[0]) {                             // kung may natirang line pagkatapos ng loop...
        DrawText(line, x, y, fs, c);           // ...iguhit din
        y += fs + 6;                           // ...bumaba ulit
    }
    return y;                                  // ibalik ang y sa ilalim ng huling linya, para madugtungan ng susunod na text
}

void fmtMoney(long v, char *out, size_t n) {                     // ginagawang "PHP 1,234,567" ang numero
    long m = v / 1000000, k = (v / 1000) % 1000, u = v % 1000;   // hatiin sa millions, thousands, at units
    if (m) snprintf(out, n, "PHP %ld,%03ld,%03ld", m, k, u);     // may million: tatlong parte, may zero-padding
    else if (k) snprintf(out, n, "PHP %ld,%03ld", k, u);         // thousands lang: dalawang parte
    else snprintf(out, n, "PHP %ld", u);                         // wala pang libo: units lang
}

void drawTrustBar(int x, int y, int w, int h, int value) {        // iginuguhit ang trust meter bar
    DrawRectangle(x, y, w, h, COL_PANEL2);                        // background ng bar (walang laman)
    Color fill = value < 35 ? COL_RED : (value < TARGET_TRUST ? COL_AMBER : COL_GREEN); // red pag delikado, amber pag nasa gitna, green pag abot na sa goal
    DrawRectangle(x, y, w * value / 100, h, fill);                // punuan base sa value (porsyento ng lapad)
    int gx = x + w * TARGET_TRUST / 100;                          // position ng goal line (85%)
    DrawRectangle(gx - 1, y - 5, 3, h + 10, WHITE);               // puting marker ng goal, mas mataas ng konti sa bar
    DrawRectangleLinesEx((Rectangle){ (float)x, (float)y, (float)w, (float)h }, 2, COL_DIM); // border sa paligid ng bar
}

void drawChip(int x, int y, int w, int h, const char *code, int on) { // iginuguhit ang maliit na service chip (MH, CU, etc.)
    DrawRectangle(x, y, w, h, on ? COL_BLUE : COL_PANEL2);            // blue kung meron ang service, gray kung wala
    DrawText(code, x + (w - MeasureText(code, 16)) / 2, y + (h - 16) / 2, // text na naka-center sa chip...
             16, on ? WHITE : COL_DIM);                                // ...puti kung on, dim kung off
}

/* ---------- Screens ---------- */

void drawTopBar(void) {                                           // iginuguhit ang taas na bar (trust, patient no., lookups, streak)
    char buf[96];                                                 // pansamantalang lagayan ng text
    DrawRectangle(0, 0, SCREEN_W, 96, COL_PANEL);                 // background ng top bar
    DrawText("REFERRAL DESK", 20, 10, 20, COL_MUTED);             // title sa kaliwa taas
    snprintf(buf, sizeof buf, "TRUST  %d / 100   (goal: %d)", shownTrust, TARGET_TRUST); // gawin ang trust text
    DrawText(buf, 20, 36, 20, COL_TEXT);                          // iguhit ang trust text
    drawTrustBar(20, 64, 620, 22, shownTrust);                    // iguhit ang trust bar

    int num = (screen == SCR_RESULT) ? served : served + 1;       // numero ng pasyente: sa result screen, yung kakatapos; kung hindi, yung kasalukuyan
    if (num > PATIENTS_PER_SHIFT) num = PATIENTS_PER_SHIFT;       // wag lumampas sa total na pasyente
    snprintf(buf, sizeof buf, "Patient %d of %d", num, PATIENTS_PER_SHIFT); // gawin ang patient counter text
    DrawText(buf, 700, 12, 22, COL_TEXT);                         // iguhit sa kanan
    snprintf(buf, sizeof buf, "Lookups left: %d", lookups);       // gawin ang lookups text
    DrawText(buf, 700, 42, 20, lookups > 0 ? COL_TEXT : COL_RED); // red kapag ubos na ang lookups
    snprintf(buf, sizeof buf, "Streak: %d", streak);              // gawin ang streak text
    DrawText(buf, 700, 68, 20, streak >= 3 ? COL_GREEN : COL_TEXT); // green kapag 3 o higit na ang streak
}

void drawPatientCard(void) {                          // iginuguhit ang card ng pasyente sa kaliwa
    char buf[96];                                     // pansamantalang lagayan ng text
    int x = 40, y = 126;                              // panimulang position ng laman ng card

    DrawRectangle(20, 110, 400, 500, COL_PANEL);      // background ng card
    DrawText(cur.name, x, y, 30, COL_TEXT);           // pangalan ng pasyente, malaki
    if (cur.urgent) {                                 // kung urgent...
        int bx = x + MeasureText(cur.name, 30) + 14;  // ...ilagay ang badge sa tabi ng pangalan
        DrawRectangle(bx, y + 2, 92, 28, COL_RED);    // pulang kahon ng badge
        DrawText("URGENT", bx + 10, y + 7, 18, WHITE); // text na URGENT sa loob
    }
    y += 50;                                          // baba para sa susunod na part

    DrawText(cur.numNeeds == 2 ? "Needs two things:" : "Needs:", x, y, 18, COL_MUTED); // label depende kung isa o dalawa ang need
    y += 28;                                          // baba ulit
    for (int i = 0; i < cur.numNeeds; i++) {          // loop sa bawat need
        int s = cur.needs[i];                         // service index ng need na 'to
        drawChip(x, y, 40, 26, serviceShort[s], 1);   // chip na naka-on (blue)
        DrawText(serviceList[s], x + 52, y + 3, 20, COL_TEXT); // buong pangalan ng service sa tabi ng chip
        y += 34;                                      // baba
        y = drawWrapped(serviceLines[s], x, y, 360, 17, COL_MUTED); // linya ng pasyente, naka-wrap, at kunin ang bagong y
        y += 10;                                      // konting espasyo bago ang susunod
    }

    y += 8;                                           // dagdag espasyo
    DrawText("Household income", x, y, 18, COL_MUTED); // label ng income
    y += 24;                                          // baba
    fmtMoney(cur.income, buf, sizeof buf);            // gawing "PHP xxx,xxx" ang income
    strcat(buf, " per year");                         // idugtong ang "per year"
    DrawText(buf, x, y, 22, COL_TEXT);                // iguhit ang income
    y += 30;                                          // baba
    DrawText(incomeTierLabel(cur.income), x, y, 22, COL_AMBER); // iguhit ang income tier (amber)

    if (lookupUsed) {                                 // kung nag-lookup na...
        if (lookupMatches > 0)                        // ...at may nakitang match
            snprintf(buf, sizeof buf, "Directory: %d match(es) highlighted.", lookupMatches); // sabihin kung ilan
        else                                          // ...at walang match
            snprintf(buf, sizeof buf, "Directory: no matches found.");  // sabihin na wala
        DrawText(buf, x, 578, 17, COL_GREEN);         // iguhit ang resulta sa baba ng card
    }
}

/* Draws one hospital row. Returns 1 if it was clicked. */
int drawHospitalRow(int i, int interactive) {                    // iginuguhit ang isang hospital row (i = index, interactive = pwede ba i-click)
    const Hospital *h = &hospitals[i];                           // kunin ang hospital na 'to
    Rectangle r = { 440.0f, (float)(110 + i * 56), 640.0f, 52.0f }; // hugis ng row, bumababa ng 56px kada index
    int hover = interactive && CheckCollisionPointRec(GetMousePosition(), r); // naka-hover ba ang mouse (kung interactive)
    char num[16];                                                // lagayan ng numero ng row

    DrawRectangleRec(r, hover ? COL_HOVER : COL_PANEL);          // background ng row, iba ang kulay pag hover
    if (highlight[i]) DrawRectangleLinesEx(r, 3, COL_GREEN);     // green na border kung match sa lookup

    snprintf(num, sizeof num, "%d", i + 1);                      // gawing text ang numero (nagsisimula sa 1)
    DrawText(num, (int)r.x + 14, (int)r.y + 15, 22, COL_MUTED);  // iguhit ang numero sa kaliwa
    DrawText(h->name, (int)r.x + 50, (int)r.y + 7, 19, COL_TEXT); // iguhit ang pangalan ng hospital
    DrawText(h->costCategory == FREE_LOW_COST ? "Free / Low-cost" : "High-cost", // text ng cost category...
             (int)r.x + 50, (int)r.y + 30, 16,                   // ...position at size
             h->costCategory == FREE_LOW_COST ? COL_GREEN : COL_AMBER); // ...green kung libre/mura, amber kung mahal
    if (highlight[i])                                            // kung match sa lookup...
        DrawText("MATCH", (int)r.x + 190, (int)r.y + 30, 16, COL_GREEN); // ...may "MATCH" na label

    for (int s = 0; s < NUM_SERVICES; s++)                       // loop sa lahat ng services
        drawChip((int)r.x + 414 + s * 44, (int)r.y + 13, 40, 26, // iguhit ang chip, magkakatabi pakanan...
                 serviceShort[s], h->offersService[s]);          // ...blue kung meron, gray kung wala

    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);     // 1 kung na-click ang row na 'to
}

void drawPatientScreen(int interactive) {                        // iginuguhit ang buong patient screen (interactive = 0 kapag background lang ng result)
    char label[64];                                              // text ng lookup button
    drawTopBar();                                                // iguhit ang taas na bar
    drawPatientCard();                                           // iguhit ang card ng pasyente

    int clicked = -1;                                            // -1 = wala pang na-click
    for (int i = 0; i < hospitalCount; i++)                      // loop sa lahat ng hospital row
        if (drawHospitalRow(i, interactive)) clicked = i;        // iguhit, at kung na-click, tandaan kung alin

    if (lookupUsed) snprintf(label, sizeof label, "Directory results shown");        // label pag nagamit na
    else if (lookups > 0) snprintf(label, sizeof label, "Directory lookup (%d left)", lookups); // label pag may natitira pa
    else snprintf(label, sizeof label, "No lookups left");                           // label pag ubos na

    int canLookup = interactive && lookups > 0 && !lookupUsed;   // pwede ba pindutin ang lookup button?
    if (drawButton((Rectangle){ 20, 620, 400, 50 }, label, 22, COL_BLUE, canLookup)) // iguhit ang button, at kung na-click...
        doLookup();                                              // ...gawin ang lookup

    DrawText("MH = Mental Health   CU = General Checkup   DE = Dental   LB = Laboratory   PH = Pharmacy", // legend ng mga code...
             20, 690, 16, COL_MUTED);                            // ...sa pinakababa, para di ka malito

    if (interactive && clicked >= 0) doReferral(clicked);        // kung may na-click na hospital, i-process ang referral
}

void drawResultScreen(void) {                                    // iginuguhit ang result popup pagkatapos ng referral
    char buf[32];                                                // lagayan ng "+8" o "-15"
    drawPatientScreen(0);                                        // iguhit muna ang patient screen sa likod (hindi na clickable)
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.65f)); // madilim na overlay sa buong screen

    Color accent = outcome.good ? COL_GREEN : COL_RED;           // green kung good, red kung hindi
    DrawRectangle(250, 170, 600, 400, COL_PANEL2);               // kahon ng popup
    DrawRectangleLinesEx((Rectangle){ 250, 170, 600, 400 }, 3, accent); // border ng popup, kulay depende sa resulta

    int y = drawWrapped(outcome.reaction, 280, 195, 540, 22, COL_TEXT); // reaction ng pasyente, naka-wrap
    y += 8;                                                      // konting espasyo
    for (int i = 0; i < outcome.nLines; i++)                     // loop sa extra lines (bonus etc.)
        y = drawWrapped(outcome.lines[i], 280, y, 540, 18,       // iguhit ang bawat extra line...
                        outcome.good ? COL_GREEN : COL_AMBER);   // ...green kung good, amber kung hindi

    snprintf(buf, sizeof buf, "%s%d", outcome.change >= 0 ? "+" : "", outcome.change); // gawin ang "+8" o "-15" (ang minus sign ay kusa nang kasama sa negative)
    drawCentered(buf, 550, 392, 64, accent);                     // malaking numero sa gitna ng popup
    drawCentered("trust", 550, 462, 18, COL_MUTED);              // maliit na label na "trust" sa ilalim

    int last = (trust <= 0 || served >= PATIENTS_PER_SHIFT);     // ito na ba ang huling screen bago matapos ang game?
    if (drawButton((Rectangle){ 400, 495, 300, 50 },             // iguhit ang button...
                   last ? "See results" : "Next patient", 22, COL_BLUE, 1) // ...iba ang label kung huli na
        || IsKeyPressed(KEY_ENTER))                              // ...o pwede ring Enter key
        advance();                                               // tuloy sa susunod na screen
}

void drawNoticeScreen(void) {                                    // iginuguhit ang closure notice screen
    DrawRectangle(150, 150, 800, 400, COL_PANEL);                // kahon ng notice
    DrawRectangleLinesEx((Rectangle){ 150, 150, 800, 400 }, 4, COL_AMBER); // amber na border
    drawCentered("NOTICE", SCREEN_W / 2, 185, 48, COL_AMBER);    // malaking title na NOTICE
    int y = drawWrapped(noticeText, 200, 270, 700, 28, COL_TEXT); // sinasabi kung anong hospital ang nagsara
    y += 14;                                                     // espasyo
    y = drawWrapped("This notice will not be shown again. Remember it!", 200, y, 700, 22, COL_RED); // paalala na isang beses lang 'to (memory test, charot)
    drawWrapped("Tip: a directory lookup only lists hospitals that are open.", 200, y + 10, 700, 18, COL_MUTED); // tip tungkol sa lookup
    if (drawButton((Rectangle){ 400, 470, 300, 54 }, "OK", 24, COL_BLUE, 1) // OK button...
        || IsKeyPressed(KEY_ENTER))                              // ...o Enter key
        advance();                                               // tuloy sa susunod
}

void drawTitleScreen(void) {                                     // iginuguhit ang title/intro screen
    char buf[200];                                               // lagayan ng goal text
    drawCentered("REFERRAL DESK", SCREEN_W / 2, 70, 60, COL_TEXT); // malaking title
    drawCentered("The Trust Game", SCREEN_W / 2, 140, 30, COL_AMBER); // subtitle

    int y = drawWrapped("You work the referral desk at a community health office. Send each " // intro text, part 1...
                        "patient to a hospital that is open, offers every service they need, " // ...part 2...
                        "and fits their income.", 150, 220, 800, 22, COL_TEXT); // ...part 3, at dito na ang position/size
    y += 14;                                                     // espasyo
    DrawText("Watch out:", 150, y, 22, COL_AMBER);               // heading ng mga babala
    y += 34;                                                     // baba
    y = drawWrapped("- Some patients need TWO services at once.", 170, y, 780, 20, COL_TEXT); // babala 1
    y = drawWrapped("- URGENT patients double your penalties.", 170, y, 780, 20, COL_TEXT);   // babala 2
    y = drawWrapped("- Hospitals can close mid-shift, and you only get ONE notice.", 170, y, 780, 20, COL_TEXT); // babala 3
    y += 14;                                                     // espasyo
    snprintf(buf, sizeof buf,                                    // gawin ang goal text na may numbers...
             "Finish %d patients with at least %d trust to win. Hit 0 trust and you are out. " // ...unang part...
             "You have %d directory lookups per shift.",         // ...pangalawang part...
             PATIENTS_PER_SHIFT, TARGET_TRUST, LOOKUPS_PER_SHIFT); // ...at ang mga numerong ipapasok
    drawWrapped(buf, 150, y, 800, 20, COL_MUTED);                // iguhit ang goal text

    if (drawButton((Rectangle){ 400, 560, 300, 62 }, "Start shift", 28, COL_BLUE, 1) // Start button...
        || IsKeyPressed(KEY_ENTER))                              // ...o Enter key
        resetGame();                                             // simulan ang shift
}

void drawEndScreen(void) {                                       // iginuguhit ang end screen (may dalawang page)
    char buf[300];                                               // lagayan ng message text

    if (endPage == 0) {                                          // PAGE 1: results at report
        drawCentered("SHIFT OVER", SCREEN_W / 2, 30, 38, COL_TEXT); // title

        const char *msg;                                         // mensahe sa taas
        Color mc;                                                // kulay ng mensahe
        if (trust <= 0) {                                        // kung zero ang trust...
            msg = "The community no longer trusts your referrals. You've been dismissed."; // ...tanggal ka na
            mc = COL_RED;                                        // ...pula
        } else if (trust >= TARGET_TRUST) {                      // kung abot ang goal...
            msg = "You hit the goal. The community trusts you with their care. You win!"; // ...panalo, slay
            mc = COL_GREEN;                                      // ...green
        } else {                                                 // kung natapos pero kulang...
            snprintf(buf, sizeof buf, "Shift complete, but you didn't reach %d trust. Try again!", TARGET_TRUST); // ...gawin ang message na may goal number
            msg = buf;                                           // ...gamitin ang buffer na yun
            mc = COL_AMBER;                                      // ...amber
        }
        drawCentered(msg, SCREEN_W / 2, 90, 24, mc);             // iguhit ang mensahe

        char rk[96];                                             // lagayan ng rank text
        snprintf(rk, sizeof rk, "Rank: %s", rankTitle(trust, trust > 0)); // gawin ang "Rank: ..." text
        drawCentered(rk, SCREEN_W / 2, 130, 28, COL_TEXT);       // iguhit ang rank

        drawTrustBar(150, 185, 800, 34, trust);                  // malaking trust bar para sa final score
        char stats[96];                                          // lagayan ng stats text
        snprintf(stats, sizeof stats, "Final trust: %d     Patients served: %d     Good referrals: %d", // gawin ang stats text...
                 trust, served, goodCount);                      // ...at ang mga numero
        drawCentered(stats, SCREEN_W / 2, 235, 20, COL_MUTED);   // iguhit ang stats

        DrawText("Your shift:", 60, 290, 24, COL_AMBER);         // heading ng report
        int y = 328;                                             // panimulang y ng mga report lines
        char line[300];                                          // lagayan ng bawat report line
        if (unaffordable > 0) {                                  // kung may na-refer sa di kayang bayaran...
            snprintf(line, sizeof line, "- %d patient(s) were sent somewhere they couldn't afford. " // ...gawin ang text part 1...
                     "The right referral is one that fits the person's budget.", unaffordable); // ...part 2 at ang bilang
            y = drawWrapped(line, 60, y, 980, 19, COL_TEXT) + 8; // iguhit, tapos baba na may espasyo
        }
        if (wrongService > 0) {                                  // kung may na-refer sa kulang ang service...
            snprintf(line, sizeof line, "- %d patient(s) were sent to a place missing a service they needed. " // ...text part 1...
                     "Always check that the facility offers what you need.", wrongService); // ...part 2 at ang bilang
            y = drawWrapped(line, 60, y, 980, 19, COL_TEXT) + 8; // iguhit, tapos baba
        }
        if (closedVisits > 0) {                                  // kung may na-refer sa sarado...
            snprintf(line, sizeof line, "- %d patient(s) were sent to a hospital that was closed. " // ...text part 1...
                     "Information goes out of date, so confirm before you go.", closedVisits); // ...part 2 at ang bilang
            y = drawWrapped(line, 60, y, 980, 19, COL_TEXT) + 8; // iguhit, tapos baba
        }
        if (overpay > 0) {                                       // kung may na-refer sa libreng clinic na di bagay...
            snprintf(line, sizeof line, "- %d patient(s) were sent to a free clinic that didn't fit their needs. " // ...text part 1...
                     "The best choice fits the person, not just the cost.", overpay); // ...part 2 at ang bilang
            y = drawWrapped(line, 60, y, 980, 19, COL_TEXT) + 8; // iguhit, tapos baba
        }
        if (unaffordable + wrongService + closedVisits + overpay == 0) // kung zero lahat ng mali...
            drawWrapped("- No bad referrals. Every patient left with the help they came for.", // ...pinaka-ganda, perfect run
                        60, y, 980, 19, COL_GREEN);              // ...green text

        if (drawButton((Rectangle){ 400, 640, 300, 52 }, "Continue", 24, COL_BLUE, 1) // Continue button...
            || IsKeyPressed(KEY_ENTER))                          // ...o Enter key
            endPage = 1;                                         // punta sa page 2
    } else {                                                     // PAGE 2: "why this matters"
        drawCentered("WHY THIS MATTERS", SCREEN_W / 2, 30, 38, COL_AMBER); // title ng page 2
        int y = drawWrapped("In this game, every patient had a place that could help. In real life it is " // mahabang paliwanag, part 1...
                            "the same: there are health facilities for you, whatever your income. Free and " // ...part 2...
                            "low-cost health centers and public hospitals exist so that cost does not have " // ...part 3...
                            "to stand between you and care. Many people never go because they don't know " // ...part 4...
                            "these places exist, or they assume they can't afford it. That delay can turn " // ...part 5...
                            "a small problem into a serious one.", 60, 100, 980, 22, COL_TEXT); // ...part 6, at position/size
        y += 20;                                                 // espasyo
        DrawText("Take this with you:", 60, y, 26, COL_GREEN);   // heading ng mga takeaway
        y += 40;                                                 // baba
        y = drawWrapped("- You deserve care, and help is closer than you think.", 80, y, 940, 21, COL_TEXT) + 6; // takeaway 1
        y = drawWrapped("- Start with your barangay health center. They can check you and point you to the right facility.", // takeaway 2...
                        80, y, 940, 21, COL_TEXT) + 6;           // ...position/size
        y = drawWrapped("- Mental health, dental, laboratory, and pharmacy services are part of health care too. " // takeaway 3, part 1...
                        "Don't wait until it gets worse.", 80, y, 940, 21, COL_TEXT) + 6; // ...part 2 at position/size
        y = drawWrapped("- Tell a friend or neighbor who may not know where to go.", 80, y, 940, 21, COL_TEXT) + 6; // takeaway 4

        if (drawButton((Rectangle){ 320, 640, 220, 52 }, "Play again", 24, COL_BLUE, 1) // Play again button...
            || IsKeyPressed(KEY_ENTER))                          // ...o Enter key
            resetGame();                                         // simula ulit ng bagong shift
        if (drawButton((Rectangle){ 560, 640, 220, 52 }, "Quit", 24, COL_PANEL2, 1)) // Quit button
            quitGame = 1;                                        // i-set ang quit flag, lalabas ang main loop
    }
}

/* ---------- Main ---------- */

int main(void) {                                                 // dito nagsisimula ang program
    srand((unsigned)time(NULL));                                 // seed ng randomizer gamit ang oras, kaya iba-iba bawat laro
    loadHospitals();                                             // i-load ang listahan ng mga hospital

    InitWindow(SCREEN_W, SCREEN_H, "Referral Desk: The Trust Game"); // gumawa ng window at lagyan ng title
    SetTargetFPS(60);                                            // target na 60 frames per second

    while (!WindowShouldClose() && !quitGame) {                  // main loop: tuloy hanggang isara ang window o pindutin ang Quit
        /* let the trust meter slide toward the real value */
        if (shownTrust < trust) shownTrust++;                    // kung mas mababa ang kita sa totoo, taasan ng 1 bawat frame
        else if (shownTrust > trust) shownTrust--;               // kung mas mataas ang kita sa totoo, ibaba ng 1 bawat frame

        BeginDrawing();                                          // simulan ang pagguhit ng frame
        ClearBackground(COL_BG);                                 // burahin ang lumang frame, kulayan ng background
        switch (screen) {                                        // piliin kung anong screen ang iguguhit
            case SCR_TITLE:   drawTitleScreen();      break;     // title screen
            case SCR_NOTICE:  drawNoticeScreen();     break;     // closure notice screen
            case SCR_PATIENT: drawPatientScreen(1);   break;     // patient screen (clickable)
            case SCR_RESULT:  drawResultScreen();     break;     // result popup
            case SCR_END:     drawEndScreen();        break;     // end screen
        }
        EndDrawing();                                            // tapusin ang frame at ipakita sa screen
    }

    CloseWindow();                                               // isara ang window at linisin ang resources
    return 0;                                                    // 0 = successful na natapos ang program
}
