#include "engine.h"
#include "card.h"
#include "client.h"
#include "ai.h"
#include "settings.h"
#include "scenario.h"
#include "challengemode.h"
#include "lua.hpp"
#include "banpairdialog.h"

#ifdef AUDIO_SUPPORT

#ifdef  Q_OS_WIN32
    extern irrklang::ISoundEngine *SoundEngine;
#else
    #include <phonon/MediaObject>
    #include <phonon/AudioOutput>
    extern Phonon::MediaObject *SoundEngine;
    extern Phonon::AudioOutput *SoundOutput;
#endif
#endif

#include <QFile>
#include <QStringList>
#include <QMessageBox>
#include <QDir>
#include <QLibrary>
#include <QApplication>

Engine *Sanguosha = NULL;

extern "C" {
    Package *NewStandard();
    Package *NewWind();
    Package *NewFire();
    Package *NewThicket();
    Package *NewMountain();
    Package *NewGod();
    Package *NewSP();
    Package *NewYJCM();
    Package *NewNostalgia();
    Package *NewHuangjin();
    Package *NewYitian();
    Package *NewWisdom();

    Package *NewStandardCard();
    Package *NewStandardExCard();
    Package *NewManeuvering();
    Package *NewSPCard();
    Package *NewNostalgiaCard();
    Package *NewYitianCard();
    Package *NewJoy();
    Package *NewDisaster();
    Package *NewJoyEquip();

    Scenario *NewGuanduScenario();
    Scenario *NewFanchengScenario();
    Scenario *NewCoupleScenario();
    Scenario *NewHongyanScenario();
    Scenario *NewZombieScenario();
    Scenario *NewLegendScenario();
    Scenario *NewImpasseScenario();
}

extern "C" {
    int luaopen_sgs(lua_State *);
}

Engine::Engine()
{
    Sanguosha = this;

    addPackage(NewStandard());
    addPackage(NewWind());
    addPackage(NewFire());
    addPackage(NewThicket());
    addPackage(NewMountain());
    addPackage(NewGod());
    addPackage(NewSP());
    addPackage(NewYJCM());
    addPackage(NewNostalgia());
    addPackage(NewHuangjin());
    addPackage(NewYitian());
    addPackage(NewWisdom());

    {
        Package *test_package = new Package("test");
        (new General(test_package, "sujiang", "god", 5, true, true));
        (new General(test_package, "sujiangf", "god", 5, false, true));
        addPackage(test_package);
    }

    addPackage(NewStandardCard());
    addPackage(NewStandardExCard());
    addPackage(NewManeuvering());
    addPackage(NewSPCard());
    addPackage(NewNostalgiaCard());
    addPackage(NewYitianCard());
    addPackage(NewJoy());
    addPackage(NewDisaster());
    addPackage(NewJoyEquip());

    addScenario(NewGuanduScenario());
    addScenario(NewFanchengScenario());
    addScenario(NewCoupleScenario());
    addScenario(NewHongyanScenario());
    addScenario(NewZombieScenario());
    addScenario(NewLegendScenario());
    addScenario(NewImpasseScenario());

    // available game modes
    modes["02p"] = tr("2 players");
    //modes["02pbb"] = tr("2 players (using blance beam)");
    modes["02_1v1"] = tr("2 players (KOF style)");
    modes["03p"] = tr("3 players");
    modes["04p"] = tr("4 players");
    modes["04_1v3"] = tr("4 players (Hulao Pass)");
    modes["05p"] = tr("5 players");
    modes["06p"] = tr("6 players");
    modes["06pd"] = tr("6 players (2 renegades)");
    modes["06_3v3"] = tr("6 players (3v3)");
    modes["07p"] = tr("7 players");
    modes["08p"] = tr("8 players");
    modes["08pd"] = tr("8 players (2 renegades)");
    modes["08boss"] = tr("8 players (boss mode)");
    modes["08same"] = tr("8 players (same mode)");
    modes["09p"] = tr("9 players");
    modes["10p"] = tr("10 players");

    //challenge_mode_set = NULL;
    challenge_mode_set = new ChallengeModeSet(this);
    //addPackage(challenge_mode_set);

    translations.insert("bossmode", tr("Boss mode"));

    connect(qApp, SIGNAL(aboutToQuit()), this, SLOT(deleteLater()));

    QString error_msg;
    lua = createLuaState(false, error_msg);
    if(lua == NULL){
        QMessageBox::warning(NULL, tr("Lua script error"), error_msg);
        exit(1);
    }
}

lua_State *Engine::createLuaState(bool load_ai, QString &error_msg){
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    luaopen_sgs(L);

    int error = luaL_dofile(L, "sanguosha.lua");
    if(error){
        error_msg = lua_tostring(L, -1);
        return NULL;
    }

    if(load_ai){
        error = luaL_dofile(L, "lua/ai/smart-ai.lua");
        if(error){
            error_msg = lua_tostring(L, -1);
            return NULL;
        }
    }

    return L;
}

lua_State *Engine::getLuaState() const{
    return lua;
}

void Engine::addTranslationEntry(const char *key, const char *value){
    translations.insert(key, QString::fromUtf8(value));
}

Engine::~Engine(){
    lua_close(lua);

#ifdef AUDIO_SUPPORT
    if(SoundEngine) {
#ifdef  Q_OS_WIN32
        SoundEngine->drop();
        SoundEngine = NULL;
#else
        delete SoundEngine;
#endif
    }
#endif

}

QStringList Engine::getScenarioNames() const{
    return scenarios.keys();
}

void Engine::addScenario(Scenario *scenario){
    scenarios.insert(scenario->objectName(), scenario);

    addPackage(scenario);
}

const Scenario *Engine::getScenario(const QString &name) const{
    return scenarios.value(name, NULL);
}

const ChallengeModeSet *Engine::getChallengeModeSet() const{
    return challenge_mode_set;
}

const ChallengeMode *Engine::getChallengeMode(const QString &name) const{
    return challenge_mode_set->getMode(name);
}

void Engine::addSkills(const QList<const Skill *> &all_skills){
    foreach(const Skill *skill, all_skills){
        if(skills.contains(skill->objectName()))
            QMessageBox::warning(NULL, "", tr("Duplicated skill : %1").arg(skill->objectName()));

        skills.insert(skill->objectName(), skill);

        if(skill->inherits("ProhibitSkill"))
            prohibit_skills << qobject_cast<const ProhibitSkill *>(skill);
        else if(skill->inherits("DistanceSkill"))
            distance_skills << qobject_cast<const DistanceSkill *>(skill);
    }
}

QList<const DistanceSkill *> Engine::getDistanceSkills() const{
    return distance_skills;
}

void Engine::addPackage(Package *package){
    if(findChild<const Package *>(package->objectName()))
        return;

    package->setParent(this);

    QList<Card *> all_cards = package->findChildren<Card *>();
    foreach(Card *card, all_cards){
        card->setId(cards.length());
        cards << card;

        QString card_name = card->objectName();
        metaobjects.insert(card_name, card->metaObject());
    }

    addSkills(package->getSkills());

    QList<General *> all_generals = package->findChildren<General *>();
    foreach(General *general, all_generals){
        addSkills(general->findChildren<const Skill *>());

        if(general->isHidden()){
            hidden_generals.insert(general->objectName(), general);
            continue;
        }

        if(general->isLord())
            lord_list << general->objectName();
        else
            nonlord_list << general->objectName();

        generals.insert(general->objectName(), general);
    }

    QList<const QMetaObject *> metas = package->getMetaObjects();
    foreach(const QMetaObject *meta, metas)
        metaobjects.insert(meta->className(), meta);

    patterns.unite(package->getPatterns());
    related_skills.unite(package->getRelatedSkills());
}

void Engine::addBanPackage(const QString &package_name){
    ban_package.insert(package_name);
}

QStringList Engine::getBanPackages() const{
    if(qApp->arguments().contains("-server"))
        return Config.BanPackages;
    else
        return ban_package.toList();
}

QString Engine::translate(const QString &to_translate) const{
    return translations.value(to_translate, to_translate);
}

int Engine::getRoleIndex() const{
    if(ServerInfo.GameMode == "08boss"){
        return 2;
    }else if(ServerInfo.GameMode.startsWith("@")){
        return 3;
    }else if(ServerInfo.GameMode == "06_3v3"){
        return 4;
    }else
        return 1;
}

const CardPattern *Engine::getPattern(const QString &name) const{
    return patterns.value(name, NULL);
}

QList<const Skill *> Engine::getRelatedSkills(const QString &skill_name) const{
    QList<const Skill *> skills;
    foreach(QString skill_name, related_skills.values(skill_name))
        skills << getSkill(skill_name);

    return skills;
}

const General *Engine::getGeneral(const QString &name) const{
    if(generals.contains(name))
        return generals.value(name);
    else
        return hidden_generals.value(name, NULL);
}

int Engine::getGeneralCount(bool include_banned) const{
    if(include_banned)
        return generals.size();

    int total = generals.size();
    QHashIterator<QString, const General *> itor(generals);
    while(itor.hasNext()){
        itor.next();
        const General *general = itor.value();
        if(ban_package.contains(general->getPackage()))
            total--;

        if(Config.Enable2ndGeneral && BanPair::isBanned(general->objectName()))
            total--;
    }

    return total;
}

const Card *Engine::getCard(int index) const{
    if(index < 0 || index >= cards.length())
        return NULL;
    else
        return cards.at(index);
}

Card *Engine::cloneCard(const QString &name, Card::Suit suit, int number) const{
    const QMetaObject *meta = metaobjects.value(name, NULL);
    if(meta){
        QObject *card_obj = meta->newInstance(Q_ARG(Card::Suit, suit), Q_ARG(int, number));
        card_obj->setObjectName(name);
        return qobject_cast<Card *>(card_obj);
    }else
        return NULL;
}

SkillCard *Engine::cloneSkillCard(const QString &name) const{
    const QMetaObject *meta = metaobjects.value(name, NULL);
    if(meta){
        QObject *card_obj = meta->newInstance();
        SkillCard *card = qobject_cast<SkillCard *>(card_obj);
        return card;
    }else
        return NULL;
}

QString Engine::getVersion() const{
    return "20110912";
}

QString Engine::getVersionName() const{
    return tr("MiddleAutumn");
}

QStringList Engine::getExtensions() const{
    QStringList extensions;
    QList<const Package *> packages = findChildren<const Package *>();
    foreach(const Package *package, packages){
        if(package->inherits("Scenario"))
            continue;

        extensions << package->objectName();
    }

    extensions.removeOne("challenge_modes");

    return extensions;
}

QStringList Engine::getKingdoms() const{
    static QStringList kingdoms;
    if(kingdoms.isEmpty())
        kingdoms << "wei" << "shu" << "wu" << "qun" << "god";

    return kingdoms;
}

QColor Engine::getKingdomColor(const QString &kingdom) const{
    static QMap<QString, QColor> color_map;
    if(color_map.isEmpty()){
        color_map["wei"] = QColor(0x54, 0x79, 0x98);
        color_map["shu"] = QColor(0xD0, 0x79, 0x6C);
        color_map["wu"] = QColor(0x4D, 0xB8, 0x73);
        color_map["qun"] = QColor(0x8A, 0x80, 0x7A);
        color_map["god"] = QColor(0x96, 0x94, 0x3D);
    }

    return color_map.value(kingdom);
}

QString Engine::getSetupString() const{
    int timeout = Config.OperationNoLimit ? 0 : Config.OperationTimeout;
    QString flags;
    if(Config.FreeChoose)
        flags.append("F");
    if(Config.Enable2ndGeneral)
        flags.append("S");
    if(Config.EnableScene)
        flags.append("C");
    if(Config.EnableAI)
        flags.append("A");
    if(Config.DisableChat)
        flags.append("M");

    if(Config.MaxHpScheme == 1)
        flags.append("1");
    else if(Config.MaxHpScheme == 2)
        flags.append("2");

    QString server_name = Config.ServerName.toUtf8().toBase64();
    QStringList setup_items;
    setup_items << server_name
            << Config.GameMode
            << QString::number(timeout)
            << Sanguosha->getBanPackages().join("+")
            << flags;

    return setup_items.join(":");
}

QMap<QString, QString> Engine::getAvailableModes() const{
    return modes;
}

QString Engine::getModeName(const QString &mode) const{
    if(modes.contains(mode))
        return modes.value(mode);
    else if(mode.startsWith("@"))
        return tr("%1 [Challenge mode]").arg(translate(mode));
    else
        return tr("%1 [Scenario mode]").arg(translate(mode));

    return QString();
}

int Engine::getPlayerCount(const QString &mode) const{
    if(modes.contains(mode)){
        QRegExp rx("(\\d+)");
        int index = rx.indexIn(mode);
        if(index != -1)
            return rx.capturedTexts().first().toInt();
    }else if(mode.startsWith("@")){
        // challenge mode
        const ChallengeMode *cmode = challenge_mode_set->getMode(mode);
        if(cmode)
            return cmode->getGenerals().length() * 2;
    }else{
        // scenario mode
        const Scenario *scenario = scenarios.value(mode, NULL);
        if(scenario)
            return scenario->getPlayerCount();
    }

    return -1;
}

void Engine::getRoles(const QString &mode, char *roles) const{
    int n = getPlayerCount(mode);

    if(mode == "02_1v1"){
        qstrcpy(roles, "ZN");
        return;
    }else if(mode == "04_1v3"){
        qstrcpy(roles, "ZFFF");
        return;
    }

    if(modes.contains(mode)){
        static const char *table1[] = {
            "",
            "",

            "ZF", // 2
            "ZFN", // 3
            "ZNFF", // 4
            "ZCFFN", // 5
            "ZCFFFN", // 6
            "ZCCFFFN", // 7
            "ZCCFFFFN", // 8
            "ZCCCFFFFN", // 9
            "ZCCCFFFFNN" // 10
        };

        static const char *table2[] = {
            "",
            "",

            "ZF", // 2
            "ZFN", // 3
            "ZNFF", // 4
            "ZCFFN", // 5
            "ZCFFNN", // 6
            "ZCCFFFN", // 7
            "ZCCFFFNN", // 8
            "ZCCCFFFFN", // 9
            "ZCCCFFFFNN" // 10
        };

        const char **table = mode.endsWith("d") ? table2 : table1;
        qstrcpy(roles, table[n]);
    }else if(mode.startsWith("@")){
        if(n == 8)
            qstrcpy(roles, "ZCCCNFFF");
        else if(n == 6)
            qstrcpy(roles, "ZCCNFF");
    }else{
        const Scenario *scenario = getScenario(mode);
        if(scenario)
            scenario->getRoles(roles);
    }
}

QStringList Engine::getRoleList(const QString &mode) const{
    char roles[100];
    getRoles(mode, roles);

    QStringList role_list;
    for(int i=0; roles[i] != '\0'; i++){
        QString role;
        switch(roles[i]){
        case 'Z': role = "lord"; break;
        case 'C': role = "loyalist"; break;
        case 'N': role = "renegade"; break;
        case 'F': role = "rebel"; break;
        }
        role_list << role;
    }

    return role_list;
}

int Engine::getCardCount() const{
    return cards.length();
}

QStringList Engine::getLords() const{
    QStringList lords;

    // add intrinsic lord
    foreach(QString lord, lord_list){
        const General *general = generals.value(lord);
        if(!ban_package.contains(general->getPackage()))
            lords << lord;
    }

    return lords;
}

QStringList Engine::getRandomLords() const{
    QStringList lords = getLords();

    QStringList nonlord_list;
    foreach(QString nonlord, this->nonlord_list){
        const General *general = generals.value(nonlord);
        if(ban_package.contains(general->getPackage()))
            continue;

        if(Config.Enable2ndGeneral && BanPair::isBanned(general->objectName()))
            continue;

        nonlord_list << nonlord;
    }

    qShuffle(nonlord_list);

    int i;
    const static int extra = 2;
    for(i=0; i< extra; i++)
        lords << nonlord_list.at(i);

    return lords;
}

QStringList Engine::getLimitedGeneralNames() const{
    QStringList general_names;
    QHashIterator<QString, const General *> itor(generals);
    while(itor.hasNext()){
        itor.next();
        if(!ban_package.contains(itor.value()->getPackage())){
            general_names << itor.key();
        }
    }

    return general_names;
}

QStringList Engine::getRandomGenerals(int count, const QSet<QString> &ban_set) const{
    QStringList all_generals = getLimitedGeneralNames();

    Q_ASSERT(all_generals.count() >= count);

    if(!ban_set.isEmpty()){
        QSet<QString> general_set = all_generals.toSet();
        all_generals = general_set.subtract(ban_set).toList();
    }

    // shuffle them
    qShuffle(all_generals);

    QStringList general_list = all_generals.mid(0, count);
    Q_ASSERT(general_list.count() == count);

    return general_list;
}

QList<int> Engine::getRandomCards() const{
    bool exclude_disaters = false;

    if(Config.GameMode == "06_3v3")
        exclude_disaters = Config.value("3v3/ExcludeDisasters", true).toBool();

    if(Config.GameMode == "04_1v3")
        exclude_disaters = true;

    QList<int> list;
    foreach(Card *card, cards){
        if(exclude_disaters && card->inherits("Disaster"))
            continue;

        if(!ban_package.contains(card->getPackage()))
            list << card->getId();
    }

    qShuffle(list);

    return list;
}

QString Engine::getRandomGeneralName() const{
    return generals.keys().at(qrand() % generals.size());
}

void Engine::playAudio(const QString &name) const{
    playEffect(QString("audio/system/%1.ogg").arg(name));
}

void Engine::playEffect(const QString &filename) const{
#ifdef AUDIO_SUPPORT

    if(!Config.EnableEffects)
        return;

    if(filename.isNull())
        return;

#ifdef  Q_OS_WIN32
    if(SoundEngine == NULL)
        return;

    if(SoundEngine->isCurrentlyPlaying(filename.toAscii()))
        return;
    SoundEngine->play2D(filename.toAscii());
#else
    if(SoundEngine->currentSource().fileName() == filename.toAscii()) {
        return;
    }
    SoundEngine->setCurrentSource(Phonon::MediaSource(filename));
    SoundEngine->play();
#endif


#endif
}

void Engine::playSkillEffect(const QString &skill_name, int index) const{
    const Skill *skill = skills.value(skill_name, NULL);
    if(skill)
        skill->playEffect(index);
}

void Engine::playCardEffect(const QString &card_name, bool is_male) const{
    QString path;
    if(card_name.startsWith("@")){
        QString gender = is_male ? "male" : "female";
        path = QString("audio/card/%1/%2.ogg").arg(gender).arg(card_name);
    }else{
        const Card *card = findChild<const Card *>(card_name);
        if(card)
            path = card->getEffectPath(is_male);
    }

    playEffect(path);
}

const Skill *Engine::getSkill(const QString &skill_name) const{
    return skills.value(skill_name, NULL);
}

const TriggerSkill *Engine::getTriggerSkill(const QString &skill_name) const{
    const Skill *skill = getSkill(skill_name);
    if(skill)
        return qobject_cast<const TriggerSkill *>(skill);
    else
        return NULL;
}

const ViewAsSkill *Engine::getViewAsSkill(const QString &skill_name) const{
    const Skill *skill = getSkill(skill_name);
    if(skill == NULL)
        return NULL;

    if(skill->inherits("ViewAsSkill"))
        return qobject_cast<const ViewAsSkill *>(skill);
    else if(skill->inherits("TriggerSkill")){
        const TriggerSkill *trigger_skill = qobject_cast<const TriggerSkill *>(skill);
        return trigger_skill->getViewAsSkill();
    }else
        return NULL;
}

const ProhibitSkill *Engine::isProhibited(const Player *from, const Player *to, const Card *card) const{
    foreach(const ProhibitSkill *skill, prohibit_skills){
        if(to->hasSkill(skill->objectName()) && skill->isProhibited(from, to, card))
            return skill;
    }

    return NULL;
}

int Engine::correctDistance(const Player *from, const Player *to) const{
    int correct = 0;

    foreach(const DistanceSkill *skill, distance_skills){
        correct += skill->getCorrect(from, to);
    }

    return correct;
}
