#ifndef CMUSICXML_H
#define CMUSICXML_H

#include "ocxmlwrappers.h"
#include <quazipfile.h>
#include <QStandardPaths>
#include "CommonCounters.h"

class CMIDIInstrument
{
public:
    CMIDIInstrument(){}
    CMIDIInstrument(const QString id, const int patch, const int channel)
    {
        ID = id;
        Patch = patch;
        Channel = channel;
    }
    QString ID;
    int Patch = 0;
    int Channel = 1;
};

class CNoteObject
{
public:
    CNoteObject(){}
    CNoteObject(const int start, const int duration)
    {
        Start = start;
        Ticks = duration;
    }
    int Start = 0;
    int Ticks = 0;
    void CheckAfter(int NewPointer)
    {
        if (Start > NewPointer) Start++;
    }
};

class CDuratedObject
{
public:
    CDuratedObject(){}
    CDuratedObject(const int index, const int start, const int something = 0, const int duration = 0)
    {
        Index = index;
        Start = start;
        Something = something;
        Ticks = duration;
    }
    int Index = 0;
    int Start = 0;
    int Ticks = 0;
    void CheckAfter(int NewPointer)
    {
        if (Start > NewPointer) Start++;
    }
    int Something = 0;
};

class CVoiceObject
{
public:
    CVoiceObject(){}
    QList<CDuratedObject> slurs;
    QList<CDuratedObject> beams;
    QList<CDuratedObject> wedges;
    CNoteObject lastNote;
    int beatCount = 0;
    XMLVoiceWrapper v;
    int noteStart = 0;
    void CheckStarts(const int NewPointer)
    {
        for (CDuratedObject& j : beams) j.CheckAfter(NewPointer);
        for (CDuratedObject& j : slurs) j.CheckAfter(NewPointer);
        for (CDuratedObject& j : wedges) j.CheckAfter(NewPointer);
        lastNote.CheckAfter(NewPointer);
    }
    void AddDuration(const int Duration)
    {
        for (CDuratedObject& j : beams) j.Ticks += Duration;
        for (CDuratedObject& j : slurs) j.Ticks += Duration;
        for (CDuratedObject& j : wedges) j.Ticks += Duration;
    }
    void addChild(const QString& symbolName, const QString& attrName, const QVariant& attrValue)
    {
        v.addChild(createSymbol(symbolName,attrName,attrValue));
    }
    void addChild(const QString& symbolName, const QStringList& attrNames, const QVariantList& attrValues)
    {
        v.addChild(createSymbol(symbolName,attrNames,attrValues));
    }
    void insertChild(const QString& symbolName, const QString& attrName, const QVariant& attrValue, const int index)
    {
        v.insertChild(createSymbol(symbolName,attrName,attrValue),index);
        CheckStarts(index);
    }
    void insertChild(const QString& symbolName, const QStringList& attrNames, const QVariantList& attrValues, const int index)
    {
        v.insertChild(createSymbol(symbolName,attrNames,attrValues),index);
        CheckStarts(index);
    }
private:
    inline XMLSimpleSymbolWrapper createSymbol(const QString& symbolName)
    {
#ifdef OCSYMBOLSCOLLECTION_H
        XMLSimpleSymbolWrapper w = OCSymbolsCollection::GetDefaultSymbol(symbolName);
#else
        XMLSimpleSymbolWrapper w(symbolName);
#endif
        return w;
    }
    inline XMLSimpleSymbolWrapper createSymbol(const QString& symbolName, const QString& attrName, const QVariant& attrValue)
    {
#ifdef OCSYMBOLSCOLLECTION_H
        XMLSimpleSymbolWrapper w = OCSymbolsCollection::GetDefaultSymbol(symbolName);
#else
        XMLSimpleSymbolWrapper w(symbolName);
#endif
        w.setAttribute(attrName,attrValue);
        return w;
    }
    inline XMLSimpleSymbolWrapper createSymbol(const QString& symbolName, const QStringList& attrNames, const QVariantList& attrValues)
    {
#ifdef OCSYMBOLSCOLLECTION_H
        XMLSimpleSymbolWrapper w = OCSymbolsCollection::GetDefaultSymbol(symbolName);
#else
        XMLSimpleSymbolWrapper w(symbolName);
#endif
        for (int i = 0; i < attrNames.size(); i++) w.setAttribute(attrNames[i],attrValues[i]);
        return w;
    }
};

class CMusicXMLReader : public XMLScoreWrapper
{
public:
    CMusicXMLReader();
    CMusicXMLReader(const QString& path);
    static bool openDoc(const QString& path, QDomLiteDocument& doc);
    static QDomLiteDocument openDoc(const QString& path)
    {
        QDomLiteDocument doc;
        openDoc(path,doc);
        return doc;
    }
    static bool parseMXLDoc(const QDomLiteDocument& doc, XMLScoreWrapper& score);
    bool parseMXLDoc(const QDomLiteDocument& doc)
    {
        if (!parseMXLDoc(doc,*this)) return false;
        return true;
    }
    bool Load(const QString& path)
    {
        return parseMXLDoc(openDoc(path));
    }
    static bool parseMXLFile(const QString& path, XMLScoreWrapper& score)
    {
        QDomLiteDocument doc;
        if (!openDoc(path,doc)) return false;
        return parseMXLDoc(doc,score);
    }
};

#define _DOCTYPE_ "score-partwise PUBLIC \"-//Recordare//DTD MusicXML 4.0 Partwise//EN\" \"http://www.musicxml.org/dtds/partwise.dtd\""

class CDuratedCounter {
public:
    CDuratedCounter(const XMLSymbolWrapper& element, int ticks) {
        Element = element;
        CurrentTicks = ticks;
    }
    bool flip(int ticks) {
        if (CurrentTicks <= 0) {
            CurrentTicks = 0;
            return true;
        }
        CurrentTicks -= ticks;
        return false;
    }
    int CurrentTicks = 0;
    XMLSimpleSymbolWrapper Element;
    bool Ready = true;
};

class CDuratedList {
public:
    CDuratedList() {}
    QList<CDuratedCounter> counters;
    QList<XMLSimpleSymbolWrapper> flip(int ticks) {
        QList<XMLSimpleSymbolWrapper> l;
        for (int i = counters.size() - 1; i >= 0; i--) {
            if (counters[i].flip(ticks)) {
                l.append(counters[i].Element);
                counters.removeAt(i);
            }
        }
        return l;
    }
    QList<XMLSimpleSymbolWrapper> ready() {
        QList<XMLSimpleSymbolWrapper> l;
        for (CDuratedCounter& c : counters) {
            if (c.Ready) {
                l.append(c.Element);
                c.Ready = false;
            }
        }
        return l;
    }
    QList<XMLSimpleSymbolWrapper> going() {
        QList<XMLSimpleSymbolWrapper> l;
        for (CDuratedCounter& c : counters) l.append(c.Element);
        return l;
    }
    void append(const XMLSymbolWrapper& element, int ticks) {
        CDuratedCounter c(element,ticks);
        counters.append(c);
    }
};

class CMusicXMLWriterVoice {
public:
    CMusicXMLWriterVoice(){}
    QDomLiteElement* init(const int staffindex, const int voiceindex, QDomLiteElementList& parts, QDomLiteElement* partlist, const XMLScoreWrapper& score, const XMLLayoutWrapper& layout);
    void writeSymbol(const int x, const XMLSymbolWrapper& symbol, OCCounter& counter/*, const QList<QDomLiteElementList>& midibanks*/, const XMLScoreWrapper& score);
    void NewBar(const int barcount, int& x/*, const QList<QDomLiteElementList>& midibanks*/);
    int currentMeter = 96;
    QDomLiteElement* part;
    QDomLiteElement* staff;
private:
    QString id;
    XMLVoiceWrapper voice;
    QDomLiteElement* measure;
    QDomLiteElement* attributes;
    int midibank = 0;
    int patchcount = 0;
    int midichannel = 1;

    int tupletticks = 0;
    int tupletnormal = 0;
    int normaldot = 0;
    bool tuplettriplet = false;
    QDomLiteElementList chordnotes;
    QList<int> tiepitches;
    QList<int> prevtiepitches;
    bool slurtie = false;
    bool gliss = false;
    QList<int> midipatches;
    CDuratedList duratedlist;
    //QDomLiteElement* firstmidiinstrument = nullptr;
    QDomLiteElement noteinstrument;
    QDomLiteElementList scoreinstruments;
    QDomLiteElementList midiinstruments;
    int performancetype = 0;
    int stemdirection = 0;
    bool beamslantingoff = false;
    int cuecounter = 0;
    int beamcount = 0;
    int autobeamticks = 0;
    int autobeamlimit = 24;
    int key = 0;
    int octave = 0;
    QDomLiteElementList possibleautobeamnotes;
};

class CMusicXMLWriterStaff {
public:
    QDomLiteElement* init(const int staffpos, const XMLScoreWrapper& score, const XMLLayoutWrapper layout, QDomLiteElement* partlist, QDomLiteElementList& parts);
    void writeMasterStuff(const int mastervoice, const XMLScoreWrapper& score);
    void writeVoice(const int staffpos,const int v,const XMLScoreWrapper& score);
    void writeStaffMeasures(const int staffpos, const XMLScoreWrapper& score);
    void implodeVoices();
    void removeVoiceStaffs(QDomLiteElement* partlist, QDomLiteElementList& parts);
private:
    int mastermeter = 96;
    int masterstaff;
    int addmasterstuff;
    int id;
    int voicecount;
    OCStaffCounter staffcounter;
    QList<int> x;
    QList<CMusicXMLWriterVoice> w;
};


class CMusicXMLWriter : public QDomLiteDocument
{
public:
    CMusicXMLWriter();
    CMusicXMLWriter(const XMLScoreWrapper& score, const XMLLayoutWrapper &layout);
    static bool writeMXLDoc(QDomLiteDocument& doc, const XMLScoreWrapper& score, const XMLLayoutWrapper &layout);
    bool writeMXLDoc(const XMLScoreWrapper& score, const XMLLayoutWrapper &layout)
    {
        return writeMXLDoc(*this,score, layout);
    }
    static bool saveMXLDoc(const QString& path, const XMLScoreWrapper& score, const XMLLayoutWrapper &layout) {
        QuaZip z(path);
        z.open(QuaZip::mdCreate);
        QuaZipFile container(&z);
        container.open(QIODevice::WriteOnly, QuaZipNewInfo("META-INF/container.xml"));
        QDomLiteDocument containerDoc;
        containerDoc.documentElement->fromString(QString("<container><rootfiles><rootfile full-path=\"score.xml\"/></rootfiles></container>"));
        qDebug() << containerDoc.toString();
        container.write(containerDoc.toByteArray());
        container.close();
        QuaZipFile content(&z);
        content.open(QIODevice::WriteOnly, QuaZipNewInfo("score.xml"));
        CMusicXMLWriter contentDoc(score,layout);
        //qDebug() << contentDoc.toString();
        content.write(contentDoc.toByteArray());
        content.close();
        z.close();
        return true;
    }
    static bool saveMusicXMLDoc(const QString& path, const XMLScoreWrapper& score, const XMLLayoutWrapper &layout) {
        CMusicXMLWriter contentDoc(score,layout);
        return contentDoc.save(path,true);
    }

};

#endif // CMUSICXML_H
