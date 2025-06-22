#include "cmusicxml.h"
#include <QDebug>
#include "cpitchtextconvert.h"
#include "ocsymbolscollection.h"

int voiceSearch(const QDomLiteElement* part)
{
    int m = 1;
    QString s = "1";
    for (const auto e : (const QDomLiteElementList)part->elementsByTag("voice",true))
    {
        if (e->text > s)
        {
            m = e->text.toInt();
            s = e->text;
        }
    }
    return m;
}

int staveSearch(const QDomLiteElement* part)
{
    if (const auto e = part->elementByTag("staves",true))
    {
        return e->text.toInt();
    }
    return 1;
}

CMusicXMLReader::CMusicXMLReader() : XMLScoreWrapper() {}

CMusicXMLReader::CMusicXMLReader(const QString& path) : XMLScoreWrapper()
{
    Load(path);
}

bool CMusicXMLReader::openDoc(const QString& path, QDomLiteDocument& doc)
{
    if (path.endsWith("mxl"))
    {
        QuaZip zip(path);
        if (!zip.open(QuaZip::mdUnzip)) return false;
        QuaZipFile file(&zip);
        QStringList xmlfiles;
        for (bool f=zip.goToFirstFile(); f; f=zip.goToNextFile()) {
            qDebug() << file.getActualFileName();
            if (file.getActualFileName().endsWith("container.xml"))
            {
                QDomLiteDocument d(file);
                qDebug() << d.toString();
                QDomLiteElementList rootfiles=d.documentElement->elementsByTag("rootfile",true);
                foreach(QDomLiteElement* f,rootfiles) xmlfiles.append(f->attribute("full-path"));
                break;
            }
        }
        bool ok=false;
        for (const auto& s : xmlfiles)
        {
            if (s.endsWith(".xml"))
            {
                zip.setCurrentFile(s);
                if (doc.fromFile(file)) ok = true;
                qDebug() << doc.toString();
                break;
            }
        }
        zip.close();
        return ok;
    }
    return doc.load(path);
}

bool CMusicXMLReader::parseMXLDoc(const QDomLiteDocument& doc, XMLScoreWrapper& score)
{
    score.newScore();
    XMLLayoutWrapper layout;
    layout.setName("Score");

    const auto partwise=doc.documentElement;
    if (!partwise) return false;
    const auto partlist=partwise->elementByTag("part-list");
    if (!partlist) return false;
    //const auto scoreparts=partlist->elementsByTag("score-part");
    const auto noteparts=partwise->elementsByTag("part");

    if (const auto defaults = partwise->elementByTag("defaults"))
    {
        if (const auto pagelayout = defaults->elementByTag("page-layout"))
        {
            double h = pagelayout->childValue("page-height");
            if (h > 0)
            {
                layout.Options.setScoreType(qBound<int>(1,h/800,4));
                score.ScoreOptions.setSize(6 * qBound<int>(1,h/800,4));
            }
        }
    }
    score.ScoreOptions.setView(1);
    layout.Fonts.title.setFontSize(18);
    layout.Fonts.subtitle.setFontSize(11);
    int titleIndex = 0;
    bool titleSet=false;
    for (const auto credit : (const QDomLiteElementList)partwise->elementsByTag("credit"))
    {
        if (credit->attributeValueInt("page") == 1)
        {
            for (const auto creditwords : (const QDomLiteElementList)credit->elementsByTag("credit-words"))
            {
                if (!creditwords->text.isEmpty())
                {
                    XMLTextElementWrapper t;
                    bool isSet=false;
                    const QString type = credit->childText("credit-type");
                    if (type == "title")
                    {
                        titleIndex = titleSet;
                        titleSet = true;
                    }
                    if (type == "lyricist") titleIndex = 3;
                    if (type == "composer") titleIndex = 4;
                    switch (titleIndex++)
                    {
                    case 0:
                        t=layout.Fonts.title;
                        isSet=true;
                        break;
                    case 1:
                        t=layout.Fonts.subtitle;
                        isSet=true;
                        break;
                    case 4:
                        t=layout.Fonts.composer;
                        isSet=true;
                        break;
                    default:
                        break;
                    }
                    if (isSet)
                    {
                        const double s = creditwords->attributeValue("font-size");
                        if (s>0) t.setFontSize(s);
                        if (creditwords->attribute("font-weight")=="bold") t.setBold(true);
                        if (creditwords->attribute("font-style")=="italic") t.setItalic(true);
                        QString n = creditwords->attribute("font-family");
                        if (!n.isEmpty()) t.setFontName(n);
                        t.setText(creditwords->text);
                    }
                }
            }
        }
    }

    int partNumber = 0;
    int staveNumber = 0;
    QList<QList<CMIDIInstrument> > instruments;
    CurlyBracketConstants currentCurly = CurlyBracketConstants::CBNone;
    SquareBracketConstants currentSquare = SquareBracketConstants::SBNone;
    int SquareIndex = 0;
    for (const auto part : std::as_const(partlist->childElements))
    {
        if (part->tag == "part-group")
        {
            if (part->attribute("type") == "start")
            {
                if (part->childText("group-symbol")=="brace")
                {
                    currentCurly=CurlyBracketConstants::CBBegin;
                }
                else if (part->childText("group-symbol")=="bracket")
                {
                    SquareIndex = part->attributeValueInt("number");
                    currentSquare=SquareBracketConstants::SBBegin;
                }
            }
            else if (part->attribute("type") == "stop")
            {
                auto s = score.Template.staff(score.Template.staffCount()-1);
                if (part->attributeValueInt("number") == SquareIndex)
                {
                    s.setSquareBracket(SquareBracketConstants::SBEnd);
                    currentSquare = SquareBracketConstants::SBNone;
                }
                else
                {
                    s.setCurlyBracket(CurlyBracketConstants::CBNone);
                    currentCurly = CurlyBracketConstants::CBNone;
                }
            }
        }
        else if (part->tag == "score-part")
        {
            instruments.append(QList<CMIDIInstrument>());
            auto& instlist=instruments.last();
            int staves = 1;
            const QString id = part->attribute("id");
            for (const auto part : noteparts)
            {
                if (part->attribute("id") == id)
                {
                    staves = staveSearch(part);
                    break;
                }
            }
            if (partNumber > 0) score.AddStaff(staveNumber,part->childText("part-name"));
            //auto v = score.Voice(staveNumber,0);
            score.setStaffName(staveNumber,part->childText("part-name"));
            score.setStaffAbbreviation(staveNumber,part->childText("part-abbreviation"));

            for (const auto instr : (const QDomLiteElementList)part->elementsByTag("midi-instrument"))
            {
                instlist.append(CMIDIInstrument(instr->attribute("id"),instr->childValue("midi-program"),instr->childValue("midi-channel")));
            }
            auto s = score.Template.staff(staveNumber);
            s.setCurlyBracket(currentCurly);
            s.setSquareBracket(currentSquare);
            if (staves==2)
            {
                s.setCurlyBracket(CurlyBracketConstants::CBBegin);
            }
            for (int i = 1; i < staves; i++)
            {
                staveNumber++;
                score.AddStaff(staveNumber,part->childText("part-name"));
                //auto v = score.Voice(staveNumber,0);
                score.setStaffName(staveNumber,part->childText("part-name"));
                score.setStaffAbbreviation(staveNumber,part->childText("part-abbreviation"));
            }
            staveNumber++;
            partNumber++;
        }
    }
    layout.Template.copy(score.Template);
    score.LayoutCollection.addChild(layout.xml()->clone());

    partNumber = 0;
    staveNumber = 0;
    for (const auto part : noteparts)
    {
        auto& instlist=instruments[partNumber];
        int staves = staveSearch(part);
        const int voiceCount = int((voiceSearch(part) / double(staves)) + 0.5);
        const XMLVoiceWrapper v1 = score.Voice(staveNumber,0);
        for (int i = 0; i < staves; i++)
        {
            while (voiceCount > score.NumOfVoices(staveNumber + i)) score.AddVoice(staveNumber + i);
        }
        double factor=1;
        int meterUpper=4;
        int meterLower=4;
        QVector<CVoiceObject> voices;
        voices.resize(voiceCount * staves);
        int barNumber = -1;
        QVector<CMIDIInstrument> currentInstruments;
        for (int j = 0; j < staves; j++)
        {
            for (int i = 0; i < voiceCount; i++)
            {
                voices[(j * voiceCount) + i].v = score.Voice(staveNumber + j,i);
            }
            if (instlist.size())
            {
                const auto& m = instlist.first();
                voices[(j * voiceCount)].addChild("Channel","Channel",m.Channel);
                voices[(j * voiceCount)].addChild("Patch","Patch",m.Patch);
                currentInstruments.append(m);
            }
        }
        for (const auto measure : std::as_const(part->childElements))
        {
            if (measure->tag == "measure")
            {
                for (auto& v : voices) v.noteStart = v.v.size();
                barNumber++;
                for (auto& v: voices) v.beatCount = 0;
                for (const auto item : std::as_const(measure->childElements))
                {
                    CTagCompare s1(item);
                    /*
                    if (s1.Compare("sound"))
                    {
                        if (item->attributeExists("tempo"))
                        {
                            voices[0].addChild("Tempo",{"Tempo","NoteValue","Invisible"},{item->attribute("tempo"),2,true});
                        }
                        else if (item->attribute("forward-repeat")=="yes")
                        {
                            for (int staff = 0; staff < staves; staff++)
                            {
                                for (int i = 0; i < voiceCount; i++)
                                {
                                    voices[(staff * voiceCount) + i].addChild("Repeat",{"RepeatType","Invisible"},{1,true});
                                    voices[(staff * voiceCount) + i].noteStart = voices[(staff * voiceCount) + i].v.size();
                                }
                            }
                        }
                    }
                    */
                    if (s1.Compare("barline"))
                    {
                        for (const auto type : std::as_const(item->childElements))
                        {
                            CTagCompare s2(type);
                            if (s2.Compare("repeat"))
                            {
                                if (type->attribute("direction")=="forward")
                                {
                                    for (int staff = 0; staff < staves; staff++)
                                    {
                                        voices[staff * voiceCount].addChild("Repeat","RepeatType",1);
                                        for (int i = 1; i < voiceCount; i++) voices[(staff * voiceCount) + i].addChild("Repeat",{"RepeatType","Invisible"},{1,true});
                                    }
                                }
                                else if (type->attribute("direction")=="backward")
                                {
                                    for (int staff = 0; staff < staves; staff++)
                                    {
                                        voices[staff * voiceCount].addChild("Repeat","RepeatType",0);
                                        for (int i = 1; i < voiceCount; i++) voices[(staff * voiceCount) + i].addChild("Repeat",{"RepeatType","Invisible"},{0,true});
                                    }
                                }
                            }
                        }
                    }
                    else if (s1.Compare("attributes"))
                    {
                        for (const auto attr : std::as_const(item->childElements))
                        {
                            CTagCompare s2(attr);
                            if (s2.Compare("key"))
                            {
                                const int k = attr->childValue("fifths")+6;
                                for (int staff = 0; staff < staves; staff++)
                                {
                                    voices[staff * voiceCount].addChild("Key","Key",k);
                                    for (int i = 1; i < voiceCount; i++) voices[(staff * voiceCount) + i].addChild("Key",{"Key","Invisible"},{k,true});
                                }
                            }
                            else if (s2.Compare("time"))
                            {
                                meterUpper=attr->childValue("beats");
                                meterLower=attr->childValue("beat-type");
                                for (int staff = 0; staff < staves; staff++)
                                {
                                    voices[staff * voiceCount].addChild("Time",{"Upper","Lower"},{meterUpper,meterLower});
                                    for (int i = 1; i < voiceCount; i++) voices[(staff * voiceCount) + i].addChild("Time",{"Upper","Lower","Invisible"},{meterUpper,meterLower,true});
                                }
                            }
                            else if (s2.Compare("clef"))
                            {
                                const int staff = qMax<int>(attr->attributeValueInt("number")-1,0);
                                int clef=0;
                                QString c=attr->childText("sign");
                                if (c=="F") clef = 1;
                                else if (c=="C")
                                {
                                    clef = 2;
                                    if (int(attr->childValue("line"))==4) clef = 3;
                                }
                                else if (c=="percussion")
                                {
                                    clef = 4;
                                }
                                voices[(staff * voiceCount)].addChild("Clef","Clef",clef);
                                for (int i = 1; i < voiceCount; i++) voices[(staff * voiceCount) + i].addChild("Clef",{"Clef","Invisible"},{clef,true});
                            }
                            else if (s2.Compare("transpose"))
                            {
                                const int t = attr->childValue("chromatic") + (attr->childValue("octave-change")*12);
                                for (int staff = 0; staff < staves; staff++)
                                {
                                    for (int i = 0; i < voiceCount; i++) voices[(staff * voiceCount) + i].addChild("Transpose","Transpose",t);
                                }
                            }
                            else if (s2.Compare("divisions"))
                            {
                                factor = 24 / attr->text.toDouble();
                            }
                        }
                        if (barNumber < 1) for (CVoiceObject& v : voices) v.noteStart=v.v.size();
                    }
                    else if (s1.Compare("direction"))
                    {
                        const int staff = qMax(item->childText("staff").toInt()-1,0);
                        for (const auto directionChild : std::as_const(item->childElements))
                        {
                            CTagCompare directionCompare(directionChild);
                            if (directionCompare.Compare("direction-type"))
                            {
                                const auto sound=item->elementByTag("sound");
                                if (const auto dynamics=directionChild->elementByTag("dynamics"))
                                {
                                    int sign=0;
                                    int value=90;
                                    if (dynamics->childCount("ppp")) sign=0;
                                    else if (dynamics->childCount("pp")) sign=1;
                                    else if (dynamics->childCount("p")) sign=2;
                                    else if (dynamics->childCount("mp")) sign=3;
                                    else if (dynamics->childCount("mf")) sign=4;
                                    else if (dynamics->childCount("f")) sign=5;
                                    else if (dynamics->childCount("ff")) sign=6;
                                    else if (dynamics->childCount("fff")) sign=7;
                                    value=(sign+1)*15;
                                    if (sound) if (sound->value()) value=(sound->value()*127)/150;
                                    voices[staff*voiceCount].addChild("Dynamic",{"DynamicSign","Velocity"},{sign,value});
                                    for (int i = 0; i < staves; i++)
                                    {
                                        if (i != staff) voices[i * voiceCount].addChild("Dynamic",{"DynamicSign","Velocity","Invisible"},{sign,value,true});
                                    }
                                }
                                else if (directionCompare.Compare("wedge"))
                                {
                                    const QString type = directionChild->attribute("type");
                                    if (type=="diminuendo")
                                    {
                                        voices[staff*voiceCount].wedges.append(CDuratedObject(0,voices[0].lastNote.Start,1,voices[0].lastNote.Ticks));
                                    }
                                    else if (type=="crescendo")
                                    {
                                        voices[staff*voiceCount].wedges.append(CDuratedObject(0,voices[0].lastNote.Start,0,voices[0].lastNote.Ticks));
                                    }
                                    else if (type=="stop")
                                    {
                                        for (int i = voices[staff*voiceCount].wedges.size() - 1; i >= 0; i--)
                                        {
                                            const auto& o = voices[staff*voiceCount].wedges[i];
                                            voices[staff*voiceCount].insertChild("Hairpin",{"Ticks","HairpinType","Speed"},{o.Ticks,o.Something,50},o.Start);
                                            for (int i = 0; i < staves; i++)
                                            {
                                                if (i != staff) voices[i * voiceCount].insertChild("Hairpin",{"Ticks","HairpinType","Speed","Invisible"},{o.Ticks,o.Something,50,true},o.Start);
                                            }
                                            voices[staff*voiceCount].wedges.removeAt(i);
                                        }
                                    }
                                }
                                else if (directionCompare.Compare("words"))
                                {
                                    QStringList s = {"Text"};
                                    QVariantList v = {directionChild->text};
                                    QString a = directionChild->attribute("font-family");
                                    if (!a.isEmpty())
                                    {
                                        s << "FontName";
                                        v << a;
                                    }
                                    a = directionChild->attribute("font-weight");
                                    if (a=="bold")
                                    {
                                        s << "Bold";
                                        v << true;
                                    }
                                    a = directionChild->attribute("font-style");
                                    if (a=="italic")
                                    {
                                        s << "Italic";
                                        v << true;
                                    }
                                    a = directionChild->attribute("font-size");
                                    if (!a.isEmpty())
                                    {
                                        s << "FontSize";
                                        v << a.toDouble();
                                    }
                                    voices[staff*voiceCount].addChild("Text",s,v);
                                }
                                else if (const auto pedal=directionChild->elementByTag("pedal"))
                                {
                                    const QString type = pedal->attribute("type");
                                    if (type=="start")
                                    {
                                        voices[staff*voiceCount].addChild("Pedal","PedalSign",0);
                                    }
                                    else if (type=="stop")
                                    {
                                        voices[staff*voiceCount].addChild("Pedal","PedalSign",1);
                                    }
                                }
                                if (sound)
                                {
                                    if (sound->attributeExists("tempo"))
                                    {
                                        voices[staff*voiceCount].addChild("Tempo",{"Tempo","NoteValue"},{sound->attribute("tempo"),2});
                                    }
                                    if (const auto instr=sound->elementByTag("midi-instrument"))
                                    {
                                        int c = instr->childValue("midi-program");
                                        voices[staff*voiceCount].addChild("Patch","Patch",c);
                                        for (int i = 0; i < staves; i++)
                                        {
                                            if (i != staff) voices[i * voiceCount].addChild("Patch","Patch",c);
                                        }
                                        for (CMIDIInstrument& m : currentInstruments) m = CMIDIInstrument("",c,m.Channel);

                                    }
                                }
                            }
                        }
                    }
                    else if (s1.Compare("note") && (item->attribute("print-spacing") != "no") && (item->attribute("print-object") != "no") && (item->childCount("grace")==0))
                    {
                        //int doubleDot=0;
                        int duration=item->childValue("duration")*factor;
                        int grace = item->childCount("grace");
                        int staff = qMax<int>(item->childValue("staff")-1,0);
                        int currentVoice = qMax<int>(item->childValue("voice") -1,0);
                        auto& cv = voices[currentVoice];
                        bool TieType = false;
                        for (const auto itemChild : std::as_const(item->childElements))
                        {
                            CTagCompare itemCompare(itemChild);
                            if (itemCompare.Compare("notations"))
                            {
                                for (const auto notationsChild : std::as_const(itemChild->childElements))
                                {
                                    CTagCompare notationsCompare(notationsChild);
                                    if (notationsCompare.Compare("ornaments"))
                                    {
                                        if (notationsChild->childCount("tremolo"))
                                        {
                                            cv.addChild("Tremolo","Speed",80);
                                        }
                                        else if (notationsChild->childCount("trill-mark"))
                                        {
                                            cv.addChild("Trill",{"Speed","Range","StartFromAbove"},{80,2,true});
                                        }
                                    }
                                    else if (notationsCompare.Compare("technical"))
                                    {
                                        if (notationsChild->childCount("up-bow"))
                                        {
                                            cv.addChild("Bowing","Bowing",0);
                                        }
                                        else if (notationsChild->childCount("down-bow"))
                                        {
                                            cv.addChild("Bowing","Bowing",1);
                                        }
                                    }
                                    else if (notationsCompare.Compare("articulations"))
                                    {
                                        if (notationsChild->childCount("accent"))
                                        {
                                            cv.addChild("Accent","AddToVelocity",20);
                                        }
                                        else if (notationsChild->childCount("staccato"))
                                        {
                                            cv.addChild("Length",{"PerformanceType","Legato"},{5,20});
                                        }
                                        else if (notationsChild->childCount("tenuto"))
                                        {
                                            cv.addChild("Length",{"PerformanceType","Legato"},{4,90});
                                        }
                                    }
                                    else if (notationsCompare.Compare("slur"))
                                    {
                                        if (notationsChild->attribute("type") == "start")
                                        {
                                            const int n = notationsChild->attributeValueInt("number",1);
                                            int p = 0;
                                            if (notationsChild->attribute("placement") == "above")
                                            {
                                                p = 1;
                                            }
                                            else if (notationsChild->attribute("placement") == "below")
                                            {
                                                p = 2;
                                            }
                                            cv.slurs.append(CDuratedObject(n,cv.v.size(),p));
                                        }
                                        else if (notationsChild->attribute("type") == "stop")
                                        {
                                            const int n = notationsChild->attributeValueInt("number",1);
                                            for (int i = cv.slurs.size() -1; i >= 0; i--)
                                            {
                                                const auto& o = cv.slurs[i];
                                                if (o.Index == n)
                                                {
                                                    cv.insertChild("Slur",{"Ticks","Direction"},{o.Ticks,o.Something},o.Start);
                                                    cv.slurs.removeAt(i);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            if (itemCompare.Compare("stem"))
                            {
                                if (itemChild->text == "down")
                                {
                                    cv.addChild("DuratedBeamDirection",{"Direction","Ticks"},{1,duration});
                                }
                                else if (itemChild->text == "up")
                                {
                                    cv.addChild("DuratedBeamDirection",{"Direction","Ticks"},{0,duration});
                                }
                            }
                            if (itemCompare.Compare("beam"))
                            {
                                if (itemChild->attributeValueInt("number")==1)
                                {
                                    if (itemChild->text == "begin")
                                    {
                                        cv.beams.append(CDuratedObject(0,cv.v.size()));
                                    }
                                    else if (itemChild->text == "end")
                                    {
                                        for (int i = cv.beams.size() -1; i >= 0; i--)
                                        {
                                            const auto& o = cv.beams[i];
                                            cv.insertChild("Beam","Ticks",o.Ticks,o.Start);
                                            cv.beams.removeAt(i);
                                        }
                                    }
                                }
                            }
                            if (itemCompare.Compare("tie"))
                            {
                                if (itemChild->attribute("type")=="start") TieType = true;
                            }
                            if (itemCompare.Compare("instrument"))
                            {
                                if (!currentInstruments.isEmpty())
                                {
                                    QString id = itemChild->attribute("id");
                                    if (!id.isEmpty())
                                    {
                                        if (currentInstruments[staff].ID != id)
                                        {
                                            for (const auto& i : instlist)
                                            {
                                                if (i.ID == id)
                                                {
                                                    cv.addChild("Channel","Channel",i.Channel);
                                                    cv.addChild("Patch","Patch",i.Patch);
                                                    currentInstruments[staff] = i;
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        if (duration)
                        {
                            if (!item->childCount("chord"))
                            {
                                cv.beatCount += duration;
                                cv.lastNote = CNoteObject(cv.v.size(),duration);
                                cv.AddDuration(duration);
                            }
                            int noteValue;
                            int dotted;
                            bool triplet;
                            const int TieAdd = (TieType) ? 1 : 0;
                            XMLSimpleSymbolWrapper::ticksToNoteValue(noteValue,dotted,triplet,duration);
                            auto p = item->elementByTag("pitch");
                            if (!p) p = item->elementByTag("unpitched");
                            if (p)
                            {
                                int pitch = CPitchTextConvert::text2Pitch(p->childText("step") + p->childText("octave"))+p->childValue("alter");
                                if (pitch < 13)
                                {
                                    pitch = CPitchTextConvert::text2Pitch(p->childText("display-step") + p->childText("display-octave"))+p->childValue("display-alter");
                                }
                                const bool isChord = (item->childCount("chord"));
                                if (isChord)
                                {
                                    const int i = cv.lastNote.Start;
                                    cv.insertChild("Note",{"NoteValue","Triplet","Dotted","Pitch","NoteType"},{noteValue,triplet,dotted,pitch,2+TieAdd},i);
                                }
                                else
                                {
                                    cv.addChild("Note",{"NoteValue","Triplet","Dotted","Pitch","NoteType"},{noteValue,triplet,dotted,pitch,TieAdd});
                                }
                            }
                            else if (item->childCount("rest"))
                            {
                                if (duration == meterUpper*(96/meterLower)) {
                                    cv.addChild("Rest","NoteValue",7);
                                }
                                else
                                {
                                    cv.addChild("Rest",{"NoteValue","Triplet","Dotted"},{noteValue,triplet,dotted});
                                }
                            }
                        }
                        if (grace) {
                            static const QStringList typelist = {"whole","half","quarter","eighth","16th","32nd","64th","whole"};
                            const int noteValue = typelist.indexOf(item->elementByTag("type")->text);
                            const int dotted = item->childCount("dot");
                            const bool triplet = false;
                            const int TieAdd = (TieType) ? 5 : 4;
                            auto p = item->elementByTag("pitch");
                            if (!p) p = item->elementByTag("unpitched");
                            if (p)
                            {
                                int pitch = CPitchTextConvert::text2Pitch(p->childText("step") + p->childText("octave"))+p->childValue("alter");
                                if (pitch < 13)
                                {
                                    pitch = CPitchTextConvert::text2Pitch(p->childText("display-step") + p->childText("display-octave"))+p->childValue("display-alter");
                                }
                                cv.addChild("Note",{"NoteValue","Triplet","Dotted","Pitch","NoteType"},{noteValue,triplet,dotted,pitch,TieAdd});
                            }
                        }
                    }
                }
                const int meter=meterUpper*(96/meterLower);
                if (measure->attribute("implicit")=="yes")
                {
                    int m = 0;
                    for (auto& v : voices) m = qMax<int>(v.beatCount,m);
                    if (m <= meter)
                    {
                        for (auto& v : voices)
                        {
                            if (m-v.beatCount > 0)
                            {
                                bool triplet;
                                int dotted, noteVal;
                                XMLSimpleSymbolWrapper::ticksToNoteValue(noteVal,dotted,triplet,m-v.beatCount);
                                v.insertChild("Rest",{"NoteValue","Dotted","Triplet","Invisible"},{noteVal,dotted,triplet,true},v.noteStart);
                                v.beatCount = m;
                            }
                            int l = meterLower;
                            double newMeter = m/(96.0/l);
                            while (int(newMeter)==0)
                            {
                                l *= 2;
                                newMeter = m/(96.0/l);
                            }
                            v.insertChild("Time",{"Upper","Lower","Invisible"},{newMeter,l,true},v.noteStart);
                            v.addChild("Time",{"Upper","Lower","Invisible"},{meterUpper,meterLower,true});
                        }
                    }
                }
                else
                {
                    const int b = voices[0].beatCount;
                    if (b != meter) // short bar
                    {
                        if (b == 0)
                        {
                            for (int i = 1; i < voices.size(); i++)
                            {
                                auto& v = voices[i];
                                v.addChild("Rest",{"NoteValue","Invisible"},{7,true});
                            }
                        }
                        else
                        {
                            int l = meterLower;
                            double newMeter = b/(96.0/l);
                            while (int(newMeter)==0)
                            {
                                l *= 2;
                                newMeter = b/(96.0/l);
                            }
                            voices[0].insertChild("Time",{"Upper","Lower","Invisible"},{newMeter,l,true},voices[0].noteStart);
                            voices[0].addChild("Time",{"Upper","Lower","Invisible"},{meterUpper,meterLower,true});
                            for (int i = 1; i < voices.size(); i++)
                            {
                                auto& v = voices[i];
                                v.insertChild("Time",{"Upper","Lower","Invisible"},{newMeter,l,true},v.noteStart);
                                if (v.beatCount != b)
                                {
                                    bool triplet;
                                    int dotted, noteVal;
                                    XMLSimpleSymbolWrapper::ticksToNoteValue(noteVal,dotted,triplet,b-v.beatCount);
                                    v.addChild("Rest",{"NoteValue","Dotted","Triplet","Invisible"},{noteVal,dotted,triplet,true});
                                    v.beatCount=b;
                                }
                                v.addChild("Time",{"Upper","Lower","Invisible"},{meterUpper,meterLower,true});
                            }
                        }
                    }
                    else // normal bar
                    {
                        for (int i = 1; i < voices.size(); i++)
                        {
                            auto& v = voices[i];
                            if (v.beatCount == 0)
                            {
                                if (b==meter)
                                {
                                    v.addChild("Rest",{"NoteValue","Invisible"},{7,true});
                                }
                                else
                                {
                                    bool triplet;
                                    int dotted, noteVal;
                                    XMLSimpleSymbolWrapper::ticksToNoteValue(noteVal,dotted,triplet,b);
                                    v.addChild("Rest",{"NoteValue","Dotted","Triplet","Invisible"},{noteVal,dotted,triplet,true});
                                }
                                v.beatCount=b;
                            }
                            else if (v.beatCount != b)
                            {
                                bool triplet;
                                int dotted, noteVal;
                                XMLSimpleSymbolWrapper::ticksToNoteValue(noteVal,dotted,triplet,b-v.beatCount);
                                v.addChild("Rest",{"NoteValue","Dotted","Triplet","Invisible"},{noteVal,dotted,triplet,true});
                                v.beatCount=b;
                            }
                        }
                    }
                }
            }
        }
        partNumber++;
        staveNumber+=staves;
    }
    for (int i = 0; i < score.NumOfStaffs(); i++)
    {
        XMLStaffWrapper s = score.Staff(i);
        for (int j = s.voiceCount() - 1; j >= 0; j--)
        {
            if (s.voiceCount() > 1)
            {
                if (s.XMLVoice(j).isFullRest()) s.deleteChild(j);
            }
        }
    }
    return true;
}

const QList<XMLSimpleSymbolWrapper> lookBack(const XMLVoiceWrapper& v, int p) {
    QList<XMLSimpleSymbolWrapper> l;
    for (int i = p - 1; i >= 0; i--) {
        XMLSimpleSymbolWrapper s = v.XMLSimpleSymbol(i);
        if (s.IsValuedRestOrValuedNote()) return l;
        l << s;
    }
    return l;
}

const QList<XMLSimpleSymbolWrapper> lookForward(const XMLVoiceWrapper& v, int p) {
    QList<XMLSimpleSymbolWrapper> l;
    for (int i = p + 1; i < v.symbolCount(); i++) {
        XMLSimpleSymbolWrapper s = v.XMLSimpleSymbol(i);
        if (s.IsValuedRestOrValuedNote()) return l;
        l << s;
    }
    return l;
}

QDomLiteElement* addCredit(QDomLiteDocument& doc, const QString& type, const XMLTextElementWrapper& text, const double x, const double y, const QString& justify = "center", const QString& valign = "top") {
    QDomLiteElement* e = nullptr;
    if (!text.text().isEmpty()) {
        QDomLiteElement* credit = doc.documentElement->appendChild("credit","page",1);
        credit->appendChild("credit-type")->text = type;
        e = credit->appendChild("credit-words");
        e->setAttribute("default-x",x);
        e->setAttribute("default-y",y);
        e->setAttribute("justify",justify);
        e->setAttribute("valign",valign);
        e->setAttributes({"font-family","font-size","font-weight"},{text.fontName(),text.fontSize(),text.bold() ? "bold" : "normal"});
        e->text = text.text();
    }
    return e;
}

#define notationselement note->elementByTagCreate("notations")
#define articulationselement notationselement->elementByTagCreate("articulations")
#define ornamentselement notationselement->elementByTagCreate("ornaments")
#define technicalelement notationselement->elementByTagCreate("technical")
#define directionelement measure->appendChild("direction")
#define directionbelow measure->appendChild("direction","placement","below")
//#define directionabove measure->appendChild("direction","placement","above")
#define directiontypeelement directionelement->appendChild("direction-type")
#define directiontypebelow directionbelow->appendChild("direction-type")
//#define directiontypeabove directionabove->appendChild("direction-type")
#define appenddirection QDomLiteElement* direction = measure->appendChild("direction")
#define appenddirectionbelow QDomLiteElement* direction = measure->appendChild("direction","placement","below")
#define appenddirectiontype direction->appendChild("direction-type")
#define vissym if (symbol.isVisible())
#define audsym if (symbol.isAudible())

QDomLiteElement* wordselement(const QString& text, const XMLFontWrapper& font, const double fontfactor = 1) {
    auto e = new QDomLiteElement("words",{"font-family","font-size","font-weight","font-style"},{font.fontName(),font.fontSize()*0.066*fontfactor,font.bold() ? "bold" : "normal",font.italic() ? "italic" : "normal"});
    e->text = text;
    return e;
}

QDomLiteElement* midiinstrumentelement(const QString& pid, int program, int channel, int bank) {
    QDomLiteElement* i = new QDomLiteElement("midi-instrument","id",pid);
    i->appendChild("midi-program")->text = program;
    i->appendChild("midi-channel")->text = channel;
    i->appendChild("midi-bank")->text = bank;
    return i;
}

QDomLiteElement* scoreinstrumentelement(const QString& pid, int patch, int midibank/*, const QList<QDomLiteElementList>& midibanks*/) {
    QDomLiteElement* s = new QDomLiteElement("score-instrument","id",pid);
    if (!midibank) s->appendChild("instrument-name")->text = CPatch::PatchList[midibank][patch-1];
    s->appendChild("instrument-sound")->text = "midi.instrument";
    return s;
}

QDomLiteElement* CMusicXMLWriterVoice::init(const int staffindex, const int voiceindex, QDomLiteElementList& parts, QDomLiteElement* partlist, const XMLScoreWrapper& score, const XMLLayoutWrapper& layout) {
    id = "P" + QString::number(staffindex+1) + "-V" + QString::number(voiceindex+1);
    const XMLTemplateStaffWrapper templatestaff = layout.Template.staff(staffindex);
    part = partlist->appendChild("score-part","id",id);
    part->appendChild("part-name")->text = score.StaffName(templatestaff.id());
    staff = new QDomLiteElement("part","id",id);
    parts.append(staff);
    voice = score.Staff(templatestaff.id()).XMLVoice(voiceindex);
    measure = staff->appendChild("measure","number",1);
    attributes = measure->appendChild("attributes");
    attributes->appendChild("divisions")->text = "24";
    if (templatestaff.size()) {
        QDomLiteElement* staffdetails = attributes->appendChild("staff-details");
        staffdetails->appendChild("staff-lines")->text = 5;
        staffdetails->appendChild("staff-size")->text = 100 + (templatestaff.size()*4);
    }
    for (int x = 0; x < voice.symbolCount(); x++) {
        const XMLSimpleSymbolWrapper symbol = voice.XMLSimpleSymbol(x);
        if (symbol.Compare("Patch")) patchcount++;
        if (patchcount > 1) break;
    }
    for (int x = 0; x < voice.symbolCount(); x++) {
        const XMLSimpleSymbolWrapper symbol = voice.XMLSimpleSymbol(x);
        if (symbol.Compare("Channel")) {
            midichannel = symbol.getIntVal("Channel");
            if (midichannel == 10) midibank = 1;
            break;
        }
    }
    return staff;
}

void CMusicXMLWriterVoice::writeSymbol(const int x, const XMLSymbolWrapper& symbol, OCCounter& counter/*, const QList<QDomLiteElementList>& midibanks*/, const XMLScoreWrapper& score) {
    static const QStringList typelist = {"whole","half","quarter","eighth","16th","32nd","64th","whole"};
    static const QStringList hairpintypes = {"crescendo","diminuendo"};
    if (symbol.IsKey()) {
        key = symbol.getIntVal("Key") - 6;
        QDomLiteElement* keyelem = attributes->prependChild("key");
        if (!symbol.isVisible()) keyelem->setAttribute("print-object","no");
        keyelem->appendChild("fifths")->text = key;
    }
    else if (symbol.IsClef()) {
        static const QStringList sign = {"G","F","C","C","percussion"};
        static const QStringList line = {"2","4","3","4","3"};
        QDomLiteElement* clef = attributes->prependChild("clef");
        if (!symbol.isVisible()) clef->setAttribute("print-object","no");
        clef->appendChild("sign")->text = sign[symbol.getIntVal("Clef")];
        clef->appendChild("line")->text = line[symbol.getIntVal("Clef")];
    }
    else if (symbol.IsTime()) {
        QDomLiteElement* time = attributes->prependChild("time");
        if (!symbol.isVisible()) time->setAttribute("print-object","no");
        switch (symbol.getIntVal("TimeType")) {
        case 0:
            time->appendChild("beats")->text = symbol.getIntVal("Upper");
            time->appendChild("beat-type")->text = symbol.getIntVal("Lower");
            break;
        case 1:
            time->appendChild("beats")->text = 4;
            time->appendChild("beat-type")->text = 4;
            time->setAttribute("symbol","common");
            break;
        case 2:
            time->appendChild("beats")->text = 2;
            time->appendChild("beat-type")->text = 2;
            time->setAttribute("symbol","cut");
        }
        currentMeter = OCCounter::calcTime(symbol);
        autobeamlimit = CTime::CalcBeamLimit(symbol);
    }
    else if (symbol.Compare("Limit")) {
        autobeamlimit = symbol.getIntVal("SixteenthsNotes") * 6;
    }
    else if (symbol.Compare("Tempo")) {
        appenddirection;
        vissym {
            QDomLiteElement* directiontype = appenddirectiontype;
            QDomLiteElement* tempo = directiontype->appendChild("metronome");
            tempo->appendChild("beat-unit")->text = typelist[symbol.noteValue()];
            tempo->appendChild("per-minute")->text = symbol.attribute("Tempo");
        }
        audsym direction->appendChild("sound")->text = symbol.attribute("Tempo");
    }
    else if (symbol.Compare("Transpose")) {
        attributes->prependChild("transpose")->appendChild("chromatic")->text = symbol.getIntVal("Transpose");
    }
    else if (symbol.Compare("Patch")) {
        int patch = symbol.getIntVal("Patch");
        QString pid;
        if (!midipatches.contains(patch)) {
            pid = id + "-I" + QString::number(midipatches.size() + 1);
            midiinstruments.append(midiinstrumentelement(pid,patch,midichannel,midibank));
            scoreinstruments.append(scoreinstrumentelement(pid,patch,midibank));
            midipatches.append(patch);
        }
        else {
            pid = id + "-I" + QString::number(midipatches.indexOf(patch) + 1);
        }
        measure->appendChild("sound")->appendChild("instrument-change")->appendChild(scoreinstrumentelement(pid,patch,midibank));
        if (midibank && (midipatches.size() == 1)) part->elementByTagCreate("part-name")->text = CPatch::PatchList[midibank][patch-1];
        /*
    <sound>
        <instrument-change>
          <score-instrument id="P1-I2">
            <instrument-name>Akustisk bas</instrument-name>
            <instrument-sound>pluck.bass.acoustic</instrument-sound>
            </score-instrument>
          </instrument-change>
        </sound>
*/
        if (patchcount > 1) {
            noteinstrument.tag="instrument";
            noteinstrument.setAttribute("id",pid);
        }
    }
    else if (symbol.Compare("Repeat")) {
        int repeattype = symbol.getIntVal("RepeatType");
        if (repeattype == 1) {
            vissym measure->prependChild("barline")->appendChild("repeat", "direction", "forward");
        }
        else if (repeattype == 2) {
            vissym measure->prependChild("barline","location","left")->appendChild("ending",{"number","type"},{symbol.getIntVal("Volta"),"start"});
        }
    }
    else if (symbol.Compare("Segno")) {
        if (symbol.getIntVal("SegnoType")) {
            appenddirection;
            audsym direction->appendChild("sound","dalsegno","yes");
            vissym appenddirectiontype->appendChild(wordselement("D. S.",score.TempoFont));
        }
        else {
            vissym directiontypeelement->appendChild("segno");
        }
    }
    else if (symbol.Compare("Coda")) {
        if (symbol.getIntVal("CodeType")) {
            appenddirection;
            audsym direction->appendChild("sound","tocoda","yes");
            vissym appenddirectiontype->appendChild(wordselement("To Coda",score.TempoFont));
        }
        else {
            vissym directiontypeelement->appendChild("coda");
        }
    }
    else if (symbol.Compare("DaCapo")) {
        appenddirection;
        audsym direction->appendChild("sound","dacapo","yes");
        vissym appenddirectiontype->appendChild(wordselement("D. C.",score.TempoFont));
    }
    else if (symbol.Compare("Fine")) {
        appenddirection;
        audsym direction->appendChild("sound","fine","yes");
        vissym appenddirectiontype->appendChild(wordselement("Fine.",score.TempoFont));
    }
    else if (symbol.Compare("Octave")) {
        octave = symbol.getIntVal("OctaveType") - 2;
        if (octave == 0) {
            vissym directiontypeelement->appendChild("octave-shift","type","stop");
        }
        else {
            vissym directiontypeelement->appendChild("octave-shift","type",(octave < 0) ? "down" : "up");
        }
    }
    else if (symbol.Compare("Cue")) {
        if (symbol.getBoolVal("Reset")) cuecounter = 0;
        cuecounter++;
        QString t = CCue::cueletter(cuecounter);
        if (symbol.getIntVal("Type")) t = QString::number(cuecounter);
        vissym directiontypeelement->appendChild("rehearsal")->text = t;
    }
    else if (symbol.Compare("Dynamic")) {
        int d = symbol.getIntVal("DynamicSign");
        appenddirection;
        vissym appenddirectiontype->appendChild("dynamics")->appendChild(CDynamic::DynamicList[d]);
        audsym direction->appendChild("sound")->appendChild("dynamics")->text = QString::number(int(symbol.getIntVal("Velocity") * 1.27));
    }
    else if (symbol.Compare("fz")) {
        vissym directiontypeelement->appendChild("dynamics")->appendChild("fz");
    }
    else if (symbol.Compare("fp")) {
        vissym directiontypeelement->appendChild("dynamics")->appendChild("fp");
    }
    else if (symbol.Compare("DynamicChange")) {
        vissym directiontypebelow->appendChild(wordselement(CDynChange::DynamicChangeList[symbol.getIntVal("DynamicType")],score.DynamicFont));
    }
    else if (symbol.Compare("TempoChange")) {
        vissym directiontypeelement->appendChild(wordselement(CTempoChange::TempoChangeList[symbol.getIntVal("TempoType")],score.TempoFont));
    }
    else if (symbol.Compare("Length")) {
        const int i = symbol.getIntVal("PerformanceType");
        if (i < 3) performancetype = i;
    }
    else if (symbol.Compare("StemDirection")) {
        const int i = symbol.getIntVal("Direction");
        stemdirection = (i == 2) ? -1 : i;
    }
    else if (symbol.Compare("BeamSlant")) {
        beamslantingoff = symbol.getBoolVal("BeamSlanting");
    }
    else if (symbol.Compare("Pedal")) {
        appenddirectionbelow;
        vissym appenddirectiontype->appendChild("pedal","type",symbol.getIntVal("PedalSign") == 0 ? "start" : "stop");
        audsym direction->appendChild("sound","damper-pedal",symbol.getIntVal("PedalSign") == 0 ? "yes" : "no");
    }
    else if (symbol.Compare("Text")) {
        vissym directiontypeelement->appendChild(wordselement(symbol.attribute("Text"),XMLFontWrapper(symbol),10));
    }
    else if (symbol.IsTuplet()) {
        counter.beginTuplet(x, voice);
        normaldot = XMLSimpleSymbolWrapper::isDotted(symbol.getIntVal("TupletValue"));
    }
    else if (symbol.isDurated()) {
        duratedlist.append(symbol,symbol.ticks());
    }
    else if (symbol.IsRestOrAnyNote() || symbol.IsAnyVorschlag()) {
        QDomLiteElement* note = new QDomLiteElement("note");
        if (!symbol.isVisible()) note->setAttribute("print-object","no");
        int perftype = performancetype;
        int stemdir = stemdirection;
        bool beamsloff = beamslantingoff;
        if (symbol.IsRestOrValuedNote()) {
            for (const XMLSimpleSymbolWrapper&s : (const QList<XMLSimpleSymbolWrapper>)duratedlist.ready()) {
                if (s.Compare("Slur")) {
                    notationselement->appendChild("slur",{"type","number"},{"start","2"});
                }
                else if (s.Compare("Hairpin")) {
                    int hairpintype = s.getIntVal("HairpinType");
                    if (hairpintype > 1) hairpintype -= 2;
                    directiontypeelement->appendChild("wedge","type",hairpintypes[hairpintype]);
                }
                else if (s.Compare("Beam")) {
                    note->appendChild("beam","number",1)->text = "begin";
                    beamcount = 0;
                }
            }
            for (const XMLSimpleSymbolWrapper&s : (const QList<XMLSimpleSymbolWrapper>)duratedlist.going()) {
                if (s.Compare("DuratedLength")) {
                    if (symbol.IsValuedNote()) {
                        const int i = s.getIntVal("PerformanceType");
                        if (i > 0) perftype = i;
                    }
                }
                else if (s.Compare("Beam")) {
                    beamcount++;
                }
                else if (s.Compare("DuratedBeamDirection")) {
                    const int i = s.getIntVal("Direction");
                    stemdir = (i == 2) ? -1 : i;
                }
                else if (s.Compare("DuratedSlant")) {
                    beamsloff = s.getIntVal("Slanting");
                }
            }
            counter.flip(symbol.ticks());
            //counter.tripletFlip(symbol);
            if (!beamcount) {
                if (symbol.noteValue() > 2) {
                    if (symbol.IsValuedNote()) {
                        if (counter.Counter.CurrentTicksRounded + autobeamticks <= autobeamlimit) {
                            if (counter.Counter.TickCounter <= currentMeter) {
                                possibleautobeamnotes.append(note);
                            }
                        }
                    }
                }
            }
            if (autobeamticks + counter.Counter.CurrentTicksRounded >= autobeamlimit || counter.Counter.TickCounter >= currentMeter) {
                //qDebug() << "autobeam" << counter.barCount() << autobeamlimit << autobeamticks << possibleautobeamnotes.size();
                if (possibleautobeamnotes.size() > 1) {
                    possibleautobeamnotes.first()->appendChild("beam","number",1)->text = "begin";
                    possibleautobeamnotes.last()->appendChild("beam","number",1)->text = "end";
                    for (int i = 1; i < possibleautobeamnotes.size() -1; i++) {
                        possibleautobeamnotes[i]->appendChild("beam","number",1)->text = "continue";
                    }
                }
                possibleautobeamnotes.clear();
            }
            autobeamticks = (autobeamticks + counter.Counter.CurrentTicksRounded) % autobeamlimit;
            if (counter.Counter.TickCounter >= currentMeter) autobeamticks = 0;
            for (const XMLSimpleSymbolWrapper&s : (const QList<XMLSimpleSymbolWrapper>)duratedlist.flip(symbol.ticks())) {
                if (s.Compare("Slur")) {
                    notationselement->appendChild("slur",{"type","number"},{"stop","2"});
                }
                else if (s.Compare("Hairpin")) {
                    directiontypeelement->appendChild("wedge","type","stop");
                }
                else if (s.Compare("Beam")) {
                    note->appendChild("beam","number",1)->text = "end";
                    beamcount = 0;
                }
            }
            if (beamcount > 1) {
                note->appendChild("beam","number",1)->text = "continue";
            }
            for (const XMLSimpleSymbolWrapper& s : lookBack(voice,x)) {
                if (s.Compare("Fermata")) {
                    notationselement->appendChild("fermata");
                }
            }
        }
        if (symbol.IsPitchedNote()) {
            const int symbolpitch = symbol.pitch();
            if (!noteinstrument.tag.isEmpty()) note->appendChild(noteinstrument.clone());
            if (gliss) {
                if (symbol.IsValuedNote()) {
                    notationselement->appendChild("slide",{"number","type","line-type"},{1,"stop","solid"});
                    gliss = false;
                }
            }
            if (slurtie) {
                if (symbol.IsValuedNote()) {
                    notationselement->appendChild("slur",{"type","number"},{"stop","1"});
                    slurtie = false;
                }
            }
            else if (prevtiepitches.contains(symbolpitch)) {
                notationselement->appendChild("tied","type","stop");
                note->appendChild("tie","type","stop");
                prevtiepitches.removeOne(symbolpitch);
            }
            if (symbol.IsValuedNote()) {
                if (!prevtiepitches.empty()) {
                    notationselement->appendChild("tied","type","stop");
                    note->appendChild("tie","type","stop");
                    prevtiepitches.clear();
                }
                for (const XMLSimpleSymbolWrapper& s : lookForward(voice,x)) {
                    if (s.Compare("Comma")) {
                        articulationselement->prependChild("breath-mark");
                    }
                }
                for (const XMLSimpleSymbolWrapper& s : lookBack(voice,x)) {
                    if (s.Compare("Accent")) {
                        articulationselement->prependChild("accent");
                    }
                    else if (s.Compare("Length")) {
                        const int i = s.getIntVal("PerformanceType") - 3;
                        if (i > 0) perftype = i;
                    }
                    else if (s.Compare("Bowing")) {
                        s.getIntVal("Bowing") ? technicalelement->prependChild("down-bow") : technicalelement->prependChild("up-bow");
                    }
                    else if (s.Compare("Tremolo")) {
                        ornamentselement->prependChild("tremolo");
                    }
                    else if (s.Compare("Trill")) {
                        ornamentselement->prependChild("trill-mark");
                    }
                    else if (s.Compare("Glissando")) {
                        notationselement->appendChild("slide",{"number","type","line-type"},{1,"start","solid"});
                        gliss = true;
                    }
                    else if (s.Compare("Fingering")) {
                        QString f = CFingering::FingerList[s.getIntVal("Finger")];
                        if (s.getBoolVal("LeadingLine")) f = "-" + f;
                        technicalelement->prependChild("fingering")->text = f;
                    }
                    else if (s.Compare("StringNumber")) {
                        technicalelement->prependChild("fingering","placement","below")->text = CStringNumber::StringSigns[s.getIntVal("String")];
                    }
                }
                if (perftype == 1) articulationselement->prependChild("tenuto");
                if (perftype == 2) articulationselement->prependChild("staccato");
                if (stemdir) note->appendChild("stem")->text = (stemdir == -1) ? "up" : "down";
                if (beamsloff) {
                    QDomLiteElement* b = note->elementByTag("beam");
                    if (b) b->setAttribute("fan","none");
                }
                measure->appendChild(note);
            }
            else if (symbol.IsCompoundNote()) {
                note->appendChild("chord");
                chordnotes << note;
            }
            else if (symbol.IsAnyVorschlag()) {
                note->appendChild("grace","slash","yes");
                measure->appendChild(note);
            }
            if (symbol.IsSingleTiedNote()) {
                if (chordnotes.empty()) {
                    int p = x;
                    bool match = false;
                    while (++p < voice.size()) {
                        const XMLSimpleSymbolWrapper s = voice.XMLSimpleSymbol(p);
                        if (s.IsAnyNote()) {
                            if (s.pitch() == symbolpitch) match = true;
                            break;
                        }
                    }
                    if (match) {
                        notationselement->appendChild("tied","type","start");
                        note->appendChild("tie","type","stop");
                        tiepitches.append(symbolpitch);
                        prevtiepitches.append(tiepitches);
                        tiepitches.clear();
                    }
                    else {
                        slurtie = true;
                        notationselement->appendChild("slur",{"type","number"},{"start","1"});
                    }
                }
                else {
                    notationselement->appendChild("tied","type","start");
                    note->appendChild("tie","type","stop");
                    tiepitches.append(symbolpitch);
                    prevtiepitches.append(tiepitches);
                    tiepitches.clear();
                }
            }
            else if (symbol.IsTiedCompoundNote()) {
                notationselement->appendChild("tied","type","start");
                note->appendChild("tie","type","stop");
                tiepitches.append(symbolpitch);
            }
            else if (symbol.IsTiedVorschlag()) {
                slurtie = true;
                notationselement->appendChild("slur",{"type","number"},{"start","1"});
            }
            QDomLiteElement* pitch = note->appendChild("pitch");
            const QString p = CPitchTextConvert::pitch2TextStep(symbolpitch,key);
            pitch->appendChild("step")->text = p.left(1);
            if (p.endsWith("#")) pitch->appendChild("alter")->text = "1";
            if (p.endsWith("b")) pitch->appendChild("alter")->text = "-1";
            pitch->appendChild("octave")->text = CPitchTextConvert::pitch2TextOctave(symbolpitch);
            if (symbol.IsAnyNote()) {
                note->appendChild("duration")->text = counter.Counter.CurrentTicksRounded;
            }
        }
        else if (symbol.IsRest()) {
            measure->appendChild(note);
            QDomLiteElement* rest = note->appendChild("rest");
            rest->appendChild("display")->text = "B";
            rest->appendChild("display-octave")->text = "4";
            if (symbol.noteValue() == 7) {
                rest->setAttribute("measure","yes");
                note->appendChild("duration")->text = QString::number(currentMeter);
            }
            else {
                note->appendChild("duration")->text = counter.Counter.CurrentTicksRounded;
            }
        }
        note->appendChild("type")->text = typelist[symbol.noteValue()];
        if (symbol.dotted()) {
            note->appendChild("dot");
            if (symbol.dotted() == 2) note->appendChild("dot");
        }
        if (symbol.IsRestOrAnyNote()) {
            if (counter.isNormalTriplet() && XMLSymbolWrapper::isStraight(symbol.ticks())) {
                note->appendChild("dot");
            }
            //if (symbol.IsRestOrValuedNote()) counter.tripletFlip(symbol);
            QDomLiteElement* timemodification = nullptr;
            {
                if (counter.isNormalTriplet()) {
                    if (symbol.IsRestOrValuedNote()) {
                        if (counter.tripletStart()) {
                            QDomLiteElement* tuplet = notationselement->appendChild("tuplet",{"number","type"},{"1","start"});
                            QDomLiteElement* tupletactual = tuplet->appendChild("tuplet-actual");
                            tupletactual->appendChild("tuplet-number")->text = 3;
                            //tupletactual->appendChild("tuplet-type")->text = "16th";
                            QDomLiteElement* tupletnormal = tuplet->appendChild("tuplet-normal");
                            tupletnormal->appendChild("tuplet-number")->text = 2;
                            //tupletnormal->appendChild("tuplet-type")->text = "16th";
                        }
                        else if (counter.tripletEnd()) {
                            notationselement->appendChild("tuplet",{"number","type"},{"1","stop"});
                        }
                    }
                    timemodification = note->appendChild("time-modification");
                    timemodification->appendChild("actual-notes")->text = 3;
                    timemodification->appendChild("normal-notes")->text = 2;
                }
                if (counter.isTuplet()) {
                    if (symbol.IsRestOrValuedNote()) {
                        if (counter.tupletStart()) {
                            QDomLiteElement* tuplet = notationselement->appendChild("tuplet",{"number","type"},{"2","start"});
                            QDomLiteElement* tupletactual = tuplet->appendChild("tuplet-actual");
                            tupletactual->appendChild("tuplet-number")->text = counter.TupletCounter.Fraction.num;
                            //tupletactual->appendChild("tuplet-type")->text = "eighth";
                            QDomLiteElement* tupletnormal = tuplet->appendChild("tuplet-normal");
                            tupletnormal->appendChild("tuplet-number")->text = counter.TupletCounter.Fraction.den;
                            //tupletnormal->appendChild("tuplet-type")->text = "eighth";
                        }
                        if (counter.tupletEnd()) {
                            notationselement->appendChild("tuplet",{"number","type"},{"2","stop"});
                        }
                    }
                    if (!timemodification) {
                        timemodification = note->appendChild("time-modification");
                        timemodification->appendChild("actual-notes")->text = counter.TupletCounter.Fraction.num;
                        timemodification->appendChild("normal-notes")->text = counter.TupletCounter.Fraction.den;
                    }
                    else {
                        QDomLiteElement* actualnotes = timemodification->elementByTagCreate("actual-notes");
                        QDomLiteElement* normalnotes = timemodification->elementByTagCreate("normal-notes");
                        const CFraction f = CFraction(actualnotes->text.toInt(),normalnotes->text.toInt()) * counter.TupletCounter.Fraction;
                        actualnotes->text = f.num;
                        normalnotes->text = f.den;
                    }
                    if (normaldot) timemodification->appendChild("normal-dot");
                }
                if (counter.isTupletTriplet()) {
                    if (symbol.IsRestOrValuedNote()) {
                        if (counter.tripletStart()) {
                            QDomLiteElement* tuplet = notationselement->appendChild("tuplet",{"number","type"},{"3","start"});
                            QDomLiteElement* tupletactual = tuplet->appendChild("tuplet-actual");
                            tupletactual->appendChild("tuplet-number")->text = 3;
                            QDomLiteElement* tupletnormal = tuplet->appendChild("tuplet-normal");
                            tupletnormal->appendChild("tuplet-number")->text = 2;
                        }
                        else if (counter.tripletEnd()) {
                            notationselement->appendChild("tuplet",{"number","type"},{"3","stop"});
                        }
                    }
                    if (!timemodification) {
                        timemodification = note->appendChild("time-modification");
                        timemodification->appendChild("actual-notes")->text = 3;
                        timemodification->appendChild("normal-notes")->text = 2;
                    }
                    else {
                        QDomLiteElement* actualnotes = timemodification->elementByTagCreate("actual-notes");
                        QDomLiteElement* normalnotes = timemodification->elementByTagCreate("normal-notes");
                        const CFraction f = CFraction(actualnotes->text.toInt(),normalnotes->text.toInt()) * CFraction(3,2);
                        actualnotes->text = f.num;
                        normalnotes->text = f.den;
                    }
                }
            }
        }
        if (symbol.IsRestOrValuedNote()) {
            counter.flip1();
            measure->appendChildren(chordnotes);
            chordnotes.clear();
        }
    }
}

void CMusicXMLWriterVoice::NewBar(const int barcount, int& x/*, const QList<QDomLiteElementList>& midibanks*/) {
        if (x < voice.symbolCount() - 1) {
            XMLSimpleSymbolWrapper s = voice.XMLSimpleSymbol(x + 1);
            const int repeattype = s.getIntVal("RepeatType");
            if (s.Compare("Repeat")) {
                if (repeattype == 0) {
                    QDomLiteElement* repeat = new QDomLiteElement("repeat", "direction", "backward");
                    measure->appendChild("barline","location","right")->appendChild(repeat);
                    int r = s.getIntVal("Repeats");
                    if (r > 2) repeat->setAttribute("times",r);
                    x++;
                }
                else if (repeattype == 3) {
                    measure->appendChild("barline","location","right")->appendChild("bar-style")->text = "light-light";
                    x++;
                }
            }
        }
        if (x < voice.symbolCount() - 1) {
            measure = staff->appendChild("measure","number",barcount + 1);
            attributes = measure->elementByTagCreate("attributes");
        }
        else {
            measure->appendChild("barline","location","right")->appendChild("bar-style")->text = "light-heavy";
            if ((midibank) && (midipatches.empty())) {
                const QString pid = id + "-I1";
                scoreinstruments.append(scoreinstrumentelement(pid,1,midibank));
                midiinstruments.append(midiinstrumentelement(pid,1,midichannel,midibank));
                part->elementByTagCreate("part-name")->text = CPatch::PatchList[midibank][0];
            }
            if (midipatches.size() > 1) {
                staff->firstChild()->elementByTagCreate("attributes")->elementByTagCreate("instruments")->text = midipatches.size();
            }
            part->appendChildren(scoreinstruments);
            part->appendChildren(midiinstruments);
        }
}

QDomLiteElement* CMusicXMLWriterStaff::init(const int staffpos, const XMLScoreWrapper& score, const XMLLayoutWrapper layout, QDomLiteElement* partlist, QDomLiteElementList& parts) {
    const XMLTemplateStaffWrapper templatestaff = layout.Template.staff(staffpos);
    id = templatestaff.id();
    voicecount = score.Staff(id).voiceCount();
    masterstaff = layout.Options.masterStaff();
    //bool masterstaffontemplate = false;
    //if (masterstaff == layout.Template.staff(0).id()) masterstaffontemplate = true;
    addmasterstuff = ((staffpos == 0) && !layout.Template.containsStaffId(masterstaff)) ? 1 : 0;
    staffcounter = OCStaffCounter(voicecount + addmasterstuff);
    x = QList<int>(voicecount + addmasterstuff,0);
    w = QList<CMusicXMLWriterVoice>(voicecount);
    for (int v = 0; v < voicecount; v++) w[v].init(staffpos,v,parts,partlist,score,layout);
    return w[0].staff;
}

void CMusicXMLWriterStaff::writeMasterStuff(const int mastervoice, const XMLScoreWrapper& score) {
    const XMLVoiceWrapper voice = score.Voice(masterstaff,0);
    OCCounter& counter = staffcounter[mastervoice];
    int& pointer = x[mastervoice];
    CMusicXMLWriterVoice& voicewriter = w[0];
    if (staffcounter[mastervoice].FragmentCounter.isReady()) {
        forever {
            const XMLSymbolWrapper symbol = voice.XMLSymbol(pointer,mastermeter);
            if (symbol.isMaster()) voicewriter.writeSymbol(pointer,symbol,counter,score);
            else if (symbol.IsRestOrValuedNote()) {
                counter.flipAll(symbol.ticks());
                staffcounter[mastervoice].FragmentCounter.setLen(counter.Counter.CurrentTicksRounded);
                if (++pointer >= voice.symbolCount()) counter.FragmentCounter.finish();
                break;
            }
            else if (symbol.IsTuplet()) {
                counter.beginTuplet(pointer,voice);
            }
            else if (symbol.IsTime()) {
                mastermeter = OCCounter::calcTime(symbol);
            }
            if (++pointer >= voice.symbolCount()) {
                counter.FragmentCounter.finish();
                break;
            }
        }
    }
}

void CMusicXMLWriterStaff::writeVoice(const int /*staffpos*/,const int v,const XMLScoreWrapper& score) {
    const int mastervoice = (addmasterstuff) ? voicecount : 0;
    const XMLVoiceWrapper voice = score.Voice(id,v);
    OCCounter& counter = staffcounter[v];
    int& pointer = x[v];
    CMusicXMLWriterVoice& voicewriter = w[v];
    if (staffcounter[v].FragmentCounter.isReady())
    {
        forever {
            const XMLSymbolWrapper symbol = voice.XMLSymbol(x[v],voicewriter.currentMeter);
            if (!(mastervoice && symbol.isMaster())) {
                voicewriter.writeSymbol(pointer,symbol,counter,score);
                if (v == 0) {
                    if (symbol.isCommon()) {
                        for (int c = 1; c < voicecount; c++) {
                            w[c].writeSymbol(x[c],symbol,staffcounter[c],score);
                        }
                    }
                }
            }
            if (symbol.IsRestOrValuedNote()) {
                staffcounter[v].FragmentCounter.setLen(counter.Counter.CurrentTicksRounded);
                if (counter.newBar(voicewriter.currentMeter)) voicewriter.NewBar(counter.barCount() + 1,pointer);
                if (++pointer >= voice.symbolCount()) counter.FragmentCounter.finish();
                break;
            }
            if (++pointer >= voice.symbolCount()) {
                counter.FragmentCounter.finish();
                break;
            }
        }
    }
}

void CMusicXMLWriterStaff::writeStaffMeasures(const int staffpos,const XMLScoreWrapper& score) {
    forever {
        const int mastervoice = (addmasterstuff) ? voicecount : 0;
        qDebug() << staffpos << addmasterstuff << mastervoice;
        if (mastervoice) writeMasterStuff(mastervoice,score);
        for (int v = 0 ; v < voicecount ; v++) writeVoice(staffpos,v,score);
        staffcounter.flip();
        if (staffcounter.isFinished() || staffcounter.newBar(w[staffcounter.firstValidVoice()].currentMeter)) {
            implodeVoices();
            if (staffcounter.isFinished()) break;
            staffcounter.barFlip();
        }
        if (staffcounter.isFinished()) break;
    }
}

void CMusicXMLWriterStaff::implodeVoices() {
    if (voicecount > 1) {
        QDomLiteElement* mainmeasure = w[0].staff->elementsByTag("measure").at(staffcounter.BarCounter);
        QDomLiteElementList afternotes;
        while (mainmeasure->lastChild()->tag != "note") afternotes.prepend(mainmeasure->takeLast());
        for (QDomLiteElement* note : (const QDomLiteElementList)mainmeasure->elementsByTag("note")) {
            note->elementByTagCreate("voice")->text = 1;
        }
        for (int v = 1; v < voicecount; v++) {
            QDomLiteElement* voicemeasure = w[v].staff->elementsByTag("measure").at(staffcounter.BarCounter);
            QDomLiteElementList voicenotes = voicemeasure->elementsByTag("note");
            qDebug() << id << mainmeasure->attribute("number") << voicemeasure->attribute("number") << voicenotes.size() << staffcounter.BarCounter;
            int backup = w[v].currentMeter;
            if (voicenotes.first()->elementByTag("rest")) {
                const int duration = voicenotes.first()->elementByTagCreate("duration")->text.numericInt();
                if (duration) {
                    backup -= duration;
                    voicenotes.removeFirst();
                }
            }
            if (backup) mainmeasure->appendChild("backup")->appendChild("duration")->text = backup;
            for (QDomLiteElement* note : std::as_const(voicenotes)) {
                QDomLiteElement* voicenote = note->clone();
                voicenote->elementByTagCreate("voice")->text = v + 1;
                mainmeasure->appendChild(voicenote);
            }
        }
        for (QDomLiteElement* e : afternotes) mainmeasure->appendChild(e);
    }

}

void CMusicXMLWriterStaff::removeVoiceStaffs(QDomLiteElement* partlist, QDomLiteElementList& parts) {
    QDomLiteElement* mainpart = partlist->childElement(partlist->childCount()-voicecount);
    for (int v = 1; v < voicecount; v++) {
        const QDomLiteElementList scoreinstruments = partlist->lastChild()->elementsByTag("score-instrument");
        const QDomLiteElementList midiinstruments = partlist->lastChild()->elementsByTag("midi-instrument");
        if (scoreinstruments.size()) {
            QDomLiteElement* e = mainpart->elementByTag("midi-instrument");
            if (e) {
                for (const QDomLiteElement* i : scoreinstruments) mainpart->insertClone(i,e);
            }
            else {
                for (const QDomLiteElement* i : scoreinstruments) mainpart->appendClone(i);
            }
            for (const QDomLiteElement* i : midiinstruments) mainpart->appendClone(i);
        }
        parts.removeLast();
        partlist->removeLast();
    }
}

CMusicXMLWriter::CMusicXMLWriter() : QDomLiteDocument(_DOCTYPE_,"score-partwise") {}

CMusicXMLWriter::CMusicXMLWriter(const XMLScoreWrapper &score, const XMLLayoutWrapper &layout) : QDomLiteDocument(_DOCTYPE_,"score-partwise") {
    writeMXLDoc(score, layout);
}

bool CMusicXMLWriter::writeMXLDoc(QDomLiteDocument &doc, const XMLScoreWrapper &score, const XMLLayoutWrapper &layout) {
    doc.documentElement->setAttribute("version","4.0");
    const double millimeters = 6.99911;
    const double tenths = 40;
    const double pageheight = 1696.94;
    const double pagewidth = 1200.48;
    const double scalefactor = tenths/millimeters;
    QDomLiteElement* defaults = doc.documentElement->appendChild("defaults");
    QDomLiteElement* scaling = defaults->appendChild("scaling");
    scaling->appendChild("millimeters")->text = millimeters;
    scaling->appendChild("tenths")->text = tenths;
    QDomLiteElement* pagelayout = defaults->appendChild("page-layout");
    pagelayout->appendChild("page-width")->text = pagewidth;
    pagelayout->appendChild("page-height")->text = pageheight;
    QDomLiteElement* lmargins = pagelayout->appendChild("page-margins");
    lmargins->appendChild("left-margin")->text = layout.Options.leftMargin() * scalefactor;
    lmargins->appendChild("right-margin")->text = layout.Options.rightMargin() * scalefactor;
    lmargins->appendChild("top-margin")->text = layout.Options.topMargin() * scalefactor;
    lmargins->appendChild("bottom-margin")->text = layout.Options.bottomMargin() * scalefactor;
    QDomLiteElement* rmargins = pagelayout->appendClone(lmargins);
    lmargins->setAttribute("type","odd");
    rmargins->setAttribute("type","even");
    double defaulty = pageheight-(layout.Options.topMargin() * scalefactor);
    XMLTextElementWrapper t = layout.Fonts.title;
    if (addCredit(doc,"title",t,pagewidth/2,defaulty)) {
        defaulty -= t.textHeight()/scalefactor;
    }
    t = layout.Fonts.subtitle;
    if (addCredit(doc,"subtitle",t,pagewidth/2,defaulty)) {
        defaulty -= t.textHeight()/scalefactor;
    }
    t = layout.Fonts.composer;
    if (addCredit(doc,"composer",t,pagewidth-(layout.Options.rightMargin() * scalefactor),defaulty,"right","bottom")) {
        //defaulty -= t.textHeight()/scalefactor;
    }
    QDomLiteElement* partlist = doc.documentElement->appendChild("part-list");
    QDomLiteElementList parts;
    int squarebracket = 0;
    int curlybracket = 0;
    for (int staffpos = 0; staffpos < layout.Template.staffCount(); staffpos++) {
        const XMLTemplateStaffWrapper templatestaff = layout.Template.staff(staffpos);
        if (templatestaff.squareBracket() == SBBegin) {
            if (squarebracket == 0) {
                QDomLiteElement* partgroup = partlist->appendChild("part-group",{"number","type"},{1,"start"});
                partgroup->appendChild("group-symbol")->text = "bracket";
                partgroup->appendChild("group-barline")->text = "yes";
            }
            squarebracket++;
        }
        if (curlybracket) curlybracket++;
        if (templatestaff.curlyBracket() == CBBegin) {
            QDomLiteElement* partgroup = partlist->appendChild("part-group",{"number","type"},{2,"start"});
            partgroup->appendChild("group-symbol")->text = "brace";
            partgroup->appendChild("group-barline")->text = "yes";
            curlybracket++;
        }

        CMusicXMLWriterStaff s;
        doc.documentElement->appendChild(s.init(staffpos,score,layout,partlist,parts));
        s.writeStaffMeasures(staffpos,score);
        s.removeVoiceStaffs(partlist,parts);

        if (templatestaff.squareBracket() == SBEnd) {
            partlist->appendChild("part-group",{"number","type"},{1,"stop"});
            squarebracket = 0;
        }
        if (curlybracket == 2) {
            partlist->appendChild("part-group",{"number","type"},{2,"stop"});
            curlybracket = 0;
        }
    }
    return true;
}

