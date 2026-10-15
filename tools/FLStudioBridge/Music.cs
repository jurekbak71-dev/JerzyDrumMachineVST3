using System.Buffers.Binary;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Text.RegularExpressions;

namespace JerzyFLBridge;

public sealed class Note {
    [JsonPropertyName("key")] public int Key { get;set; }
    [JsonPropertyName("start")] public int Start { get;set; }
    [JsonPropertyName("length")] public int Length { get;set; }
    [JsonPropertyName("velocity")] public int Velocity { get;set; }
}
public sealed class Track {
    [JsonPropertyName("name")] public string Name { get;set; }="Track";
    [JsonPropertyName("channel")] public int Channel { get;set; }
    [JsonPropertyName("division")] public string? Division {get;set;}
    [JsonPropertyName("swing")] public double? Swing {get;set;}
    [JsonPropertyName("notes")] public List<Note> Notes { get;set; }=new();
}
public sealed class Pattern {
    [JsonPropertyName("format")] public string Format {get;set;}="jerzy-pattern";
    [JsonPropertyName("version")] public int Version {get;set;}=1;
    [JsonPropertyName("name")] public string Name {get;set;}="Jerzy Pattern";
    [JsonPropertyName("ppq")] public int Ppq {get;set;}=960;
    [JsonPropertyName("bpm")] public double Bpm {get;set;}=120;
    [JsonPropertyName("time_signature")] public int[] Signature {get;set;}=new[]{4,4};
    [JsonPropertyName("tracks")] public List<Track> Tracks {get;set;}=new();
}
public static class Music {
    public const int Ppq=960;
    static readonly JsonSerializerOptions JpfJson=new(){WriteIndented=true,PropertyNameCaseInsensitive=true};
    public static Pattern Load(string file) {
        var p=JsonSerializer.Deserialize<Pattern>(File.ReadAllText(file,Encoding.UTF8),JpfJson)??throw new InvalidDataException("Nieprawidłowy JPF.");
        Validate(p);return p;
    }
    public static void Save(Pattern p,string file) { Validate(p);File.WriteAllText(file,JsonSerializer.Serialize(p,JpfJson)+"\n",new UTF8Encoding(false)); }
    static void Validate(Pattern p) {
        if(p.Format!="jerzy-pattern"||p.Version!=1||p.Ppq!=Ppq||p.Bpm<20||p.Bpm>400||p.Tracks is null||p.Tracks.Count>128)
            throw new InvalidDataException("Nieobsługiwany format JPF v1.");
        foreach(var t in p.Tracks) {
            if(t.Channel<0||t.Channel>15||t.Notes is null||t.Notes.Count>100000)throw new InvalidDataException("Uszkodzona ścieżka JPF.");
            foreach(var n in t.Notes)if(n.Key<0||n.Key>127||n.Velocity<1||n.Velocity>127||n.Start<0||n.Length<1)throw new InvalidDataException("Uszkodzona nuta JPF.");
        }
    }
    public static Pattern Drums(double bpm,int bars,int seed,bool tripletHat,double swing) {
        var rand=new Random(seed);var p=new Pattern{Name="Jerzy Drums",Bpm=bpm};
        var defs=new (string name,int key,int[] steps)[] {
            ("KICK",36,new[]{0,8,10}),("SNARE",37,new[]{4,12}),("CH",39,new[]{0,2,4,6,8,10,12,14}),
            ("OH",43,new[]{7,15}),("CLAP",41,new[]{12}),("RIM",42,new[]{6,14})};
        foreach(var d in defs) {
            bool trip=d.name=="CH"&&tripletHat;int spb=trip?6:4;int unit=Ppq/spb;
            var tr=new Track{Name=d.name,Channel=9,Division=trip?"1/16T":"1/16",Swing=trip?swing:0};
            for(int bar=0;bar<bars;bar++)foreach(int step in d.steps) {
                int s=trip?(int)Math.Round(step*spb/4.0):step;
                if((d.name=="CH"||d.name=="RIM")&&rand.NextDouble()<0.10)continue;
                int start=(bar*4*spb+s)*unit+(trip&&s%2==1?(int)Math.Round(unit*swing):0);
                tr.Notes.Add(new Note{Key=d.key,Start=start,Length=Math.Max(1,unit/2),
                    Velocity=step%4==0?112:78});
            }
            p.Tracks.Add(tr);
        }
        return p;
    }
    static readonly Dictionary<string,int> Chromatic=new(StringComparer.OrdinalIgnoreCase){
        ["C"]=0,["C#"]=1,["Db"]=1,["D"]=2,["D#"]=3,["Eb"]=3,["E"]=4,["F"]=5,
        ["F#"]=6,["Gb"]=6,["G"]=7,["G#"]=8,["Ab"]=8,["A"]=9,["A#"]=10,["Bb"]=10,["B"]=11};
    public static int[] Chord(string symbol,int octave=2) {
        var m=Regex.Match(symbol.Trim(),@"^([A-Ga-g])([#b]?)(maj7|m7|dim|aug|sus2|sus4|m|7)?$",RegexOptions.IgnoreCase);
        if(!m.Success)throw new ArgumentException("Nieznany akord: "+symbol);
        var root=Chromatic[m.Groups[1].Value.ToUpperInvariant()+m.Groups[2].Value]+(octave+1)*12;
        int[] intervals=m.Groups[3].Value.ToLowerInvariant() switch {
            "m"=>new[]{0,3,7},"maj7"=>new[]{0,4,7,11},"m7"=>new[]{0,3,7,10},
            "7"=>new[]{0,4,7,10},"dim"=>new[]{0,3,6},"aug"=>new[]{0,4,8},
            "sus2"=>new[]{0,2,7},"sus4"=>new[]{0,5,7},_=>new[]{0,4,7}
        };
        return intervals.Select(n=>n+root).ToArray();
    }
    public static Pattern Bass(double bpm,string chords,string style,int seed=71) {
        var names=chords.Split(new[]{',',';',' '},StringSplitOptions.RemoveEmptyEntries|StringSplitOptions.TrimEntries);
        if(names.Length==0||names.Length>64)throw new ArgumentException("Podaj od 1 do 64 akordów, np. Am,F,C,G.");
        var map=new Dictionary<string,(int step,int semitone)[]>(StringComparer.OrdinalIgnoreCase){
            ["Rock"]=new[]{(0,0),(2,0),(4,7),(6,0)},
            ["Dub"]=new[]{(0,0),(3,7),(6,0)},
            ["Acid"]=new[]{(0,0),(1,12),(3,7),(4,0),(7,10)},
            ["Funk"]=new[]{(0,0),(2,12),(3,7),(5,0),(7,12)},
            ["Ambient"]=new[]{(0,0),(4,7)}
        };
        if(!map.TryGetValue(style,out var steps))throw new ArgumentException("Nieznany styl basu.");
        var p=new Pattern{Name="Jerzy Bass "+style,Bpm=bpm};var rand=new Random(seed);
        var track=new Track{Name="BASS",Channel=0,Division="1/8"};
        var guide=new Track{Name="CHORD GUIDE",Channel=1,Division="1/bar"};
        foreach(var (chord,ci) in names.Select((a,i)=>(a,i))) {
            var root=Chord(chord,2)[0];int start=ci*4*Ppq;
            foreach(var key in Chord(chord,3))guide.Notes.Add(new Note{Key=key,Start=start,Length=4*Ppq-1,Velocity=64});
            foreach(var (st,interval) in steps){
                track.Notes.Add(new Note{Key=Math.Clamp(root+interval,0,127),Start=start+st*Ppq/2,
                    Length=style=="Dub"||style=="Ambient"?Ppq:Ppq/2,Velocity=rand.Next(86,114)});
            }
        }
        p.Tracks.Add(track);p.Tracks.Add(guide);return p;
    }
    public static Pattern Euclidean(double bpm,string lanes,int bars=4){
        var notes=new Dictionary<string,int>(StringComparer.OrdinalIgnoreCase){["KICK"]=36,["SNARE"]=37,["TOM"]=38,["CH"]=39,["PERC"]=40,["CLAP"]=41,["RIM"]=42,["OH"]=43,["SYNTH"]=47};
        var p=new Pattern{Name="Jerzy Euclidean",Bpm=bpm};
        foreach(var lane in lanes.Split(',',StringSplitOptions.TrimEntries|StringSplitOptions.RemoveEmptyEntries)){
            var a=lane.Split(':',StringSplitOptions.TrimEntries);
            if(a.Length!=4||!notes.TryGetValue(a[0],out int key)||!int.TryParse(a[1],out int hits)
                ||!int.TryParse(a[2],out int steps)||!int.TryParse(a[3],out int rotation)||steps<1||steps>128
                ||hits<0||hits>steps||3840%steps!=0)throw new ArgumentException("Oczekiwane ścieżki: KICK:5:16:0,CH:11:24:2");
            int unit=3840/steps;
            var tr=new Track{Name=a[0],Channel=9,Division=steps+"/bar"};
            for(int bar=0;bar<bars;bar++)for(int i=0;i<steps;i++){
                int unrotated=((i-rotation)%steps+steps)%steps;
                if((unrotated*hits)%steps>=hits)continue;
                tr.Notes.Add(new Note{Key=key,Start=(bar*steps+i)*unit,Length=Math.Max(1,unit/2),Velocity=i%4==0?112:84});
            }
            p.Tracks.Add(tr);
        }
        if(p.Tracks.Count==0)throw new ArgumentException("Brak ścieżek Euclidean.");
        return p;
    }
    public static Pattern Arp(Pattern input,int subdiv,int octaves,string direction){
        Validate(input);if(subdiv is not (2 or 3 or 4 or 6 or 8)||octaves<1||octaves>3)throw new ArgumentException("Nieprawidłowe ustawienia ARP.");
        var p=JsonSerializer.Deserialize<Pattern>(JsonSerializer.Serialize(input,JpfJson),JpfJson)!;
        foreach(var tr in p.Tracks){
            var output=new List<Note>();int unit=Ppq/subdiv;
            foreach(var group in tr.Notes.GroupBy(n=>n.Start).OrderBy(g=>g.Key)){
                var keys=group.SelectMany(n=>Enumerable.Range(0,octaves).Select(o=>n.Key+12*o))
                    .Where(n=>n<128).Distinct().OrderBy(n=>n).ToList();
                if(direction=="Down")keys.Reverse();
                if(direction=="Up-Down"&&keys.Count>2)keys.AddRange(keys.Skip(1).Take(keys.Count-2).Reverse());
                int end=group.Key+group.Max(n=>n.Length),maxVel=group.Max(n=>n.Velocity);
                for(int t=group.Key,i=0;t<end;t+=unit,i++){
                    output.Add(new Note{Key=keys[i%keys.Count],Start=t,Length=Math.Min(unit,end-t),Velocity=maxVel});
                }
            }
            tr.Notes=output;
        }
        p.Name+=" / ARP";return p;
    }
    static void BE16(Stream s,int n){s.WriteByte((byte)(n>>8));s.WriteByte((byte)n);}
    static void BE32(Stream s,int n){s.WriteByte((byte)(n>>24));s.WriteByte((byte)(n>>16));s.WriteByte((byte)(n>>8));s.WriteByte((byte)n);}
    static void VLQ(Stream s,int value){
        if(value<0)throw new ArgumentException("Ujemny czas MIDI.");
        uint b=(uint)(value&0x7f);
        while((value>>=7)>0){b<<=8;b|=(uint)((value&0x7f)|0x80);}
        while(true){s.WriteByte((byte)b);if((b&0x80)==0)break;b>>=8;}
    }
    static byte[] MakeTrack(IEnumerable<(int time,byte[] data,int order)> events){
        using var t=new MemoryStream();int time=0;
        foreach(var e in events.OrderBy(x=>x.time).ThenBy(x=>x.order)){
            VLQ(t,e.time-time);t.Write(e.data);time=e.time;
        }
        t.Write(new byte[]{0,0xff,0x2f,0});return t.ToArray();
    }
    public static void ToMidi(Pattern p,string path){
        Validate(p);using var fs=File.Create(path);fs.Write(Encoding.ASCII.GetBytes("MThd"));
        BE32(fs,6);BE16(fs,1);BE16(fs,p.Tracks.Count+1);BE16(fs,Ppq);
        int micros=Math.Clamp((int)Math.Round(60_000_000/p.Bpm),1,0xffffff);
        var meta=MakeTrack(new[]{(0,new byte[]{0xff,0x51,3,(byte)(micros>>16),(byte)(micros>>8),(byte)micros},0)});
        WriteChunk(fs,meta);
        foreach(var tr in p.Tracks){
            var ev=new List<(int time,byte[] data,int order)>();
            var title=Encoding.UTF8.GetBytes(tr.Name);
            if(title.Length>127)title=title.Take(127).ToArray();
            ev.Add((0,(new byte[]{0xff,0x03,(byte)title.Length}).Concat(title).ToArray(),0));
            foreach(var note in tr.Notes){
                ev.Add((note.Start,new byte[]{(byte)(0x90|tr.Channel),(byte)note.Key,(byte)note.Velocity},2));
                ev.Add((checked(note.Start+note.Length),new byte[]{(byte)(0x80|tr.Channel),(byte)note.Key,0},1));
            }
            WriteChunk(fs,MakeTrack(ev));
        }
    }
    static void WriteChunk(Stream fs,byte[] bytes){
        fs.Write(Encoding.ASCII.GetBytes("MTrk"));BE32(fs,bytes.Length);fs.Write(bytes);
    }
    static int Read32(BinaryReader r){
        var b=r.ReadBytes(4);if(b.Length!=4)throw new EndOfStreamException();return BinaryPrimitives.ReadInt32BigEndian(b);
    }
    static int Read16(BinaryReader r){
        var b=r.ReadBytes(2);if(b.Length!=2)throw new EndOfStreamException();return BinaryPrimitives.ReadUInt16BigEndian(b);
    }
    static int ReadVlq(BinaryReader r){
        int value=0;for(int i=0;i<4;i++){int b=r.ReadByte();value=(value<<7)|(b&127);if((b&128)==0)return value;}
        throw new InvalidDataException("Zbyt długa liczba VLQ.");
    }
    public static Pattern FromMidi(string file){
        using var fs=File.OpenRead(file);using var r=new BinaryReader(fs);
        if(Encoding.ASCII.GetString(r.ReadBytes(4))!="MThd")throw new InvalidDataException("To nie jest MIDI.");
        int len=Read32(r);if(len<6||len>1024)throw new InvalidDataException("Uszkodzony nagłówek MIDI.");
        int fmt=Read16(r),count=Read16(r),division=Read16(r);
        if(fmt>1||count>128||division<=0||(division&0x8000)!=0)throw new InvalidDataException("Obsługiwane: MIDI SMF 0/1 PPQ.");
        fs.Position+=len-6;
        var p=new Pattern{Name=Path.GetFileNameWithoutExtension(file)};
        for(int trackIndex=0;trackIndex<count;trackIndex++){
            if(Encoding.ASCII.GetString(r.ReadBytes(4))!="MTrk")throw new InvalidDataException("Brak ścieżki MTrk.");
            int bytes=Read32(r);if(bytes<0||bytes>30_000_000||fs.Position+bytes>fs.Length)throw new InvalidDataException("Zbyt duża ścieżka MIDI.");
            long end=fs.Position+bytes;int time=0,channel=0,running=0;string title="Track "+(trackIndex+1);
            var notes=new List<Note>();var active=new Dictionary<(int,int),Stack<(int start,int vel)>>();
            while(fs.Position<end){
                time=checked(time+ReadVlq(r));int status=r.ReadByte();
                if(status<0x80){fs.Position--;status=running;}
                else if(status<0xf0)running=status;
                if(status==0xff){
                    int typ=r.ReadByte(),n=ReadVlq(r);var data=r.ReadBytes(n);
                    if(typ==3)title=Encoding.UTF8.GetString(data);
                    if(typ==0x51&&n==3){int us=(data[0]<<16)|(data[1]<<8)|data[2];if(us>0)p.Bpm=60_000_000.0/us;}
                    if(typ==0x2f)break;
                    continue;
                }
                if(status==0xf0||status==0xf7){int n=ReadVlq(r);fs.Position+=n;continue;}
                if(status<0x80||status>=0xf0)throw new InvalidDataException("Nieobsługiwany komunikat MIDI.");
                int kind=status&0xf0;channel=status&0xf;
                int key=r.ReadByte(),value=kind is 0xc0 or 0xd0?0:r.ReadByte();
                if(kind==0x90&&value>0){
                    var k=(channel,key);if(!active.TryGetValue(k,out var stack))active[k]=stack=new();
                    stack.Push((time,value));
                }else if(kind==0x80||(kind==0x90&&value==0)){
                    var k=(channel,key);if(active.TryGetValue(k,out var stack)&&stack.Count>0){
                        var n=stack.Pop();int st=(int)Math.Round((double)n.start*Ppq/division),en=(int)Math.Round((double)time*Ppq/division);
                        notes.Add(new Note{Key=key,Start=st,Length=Math.Max(1,en-st),Velocity=n.vel});
                    }
                }
            }
            fs.Position=end;
            if(notes.Count>0)p.Tracks.Add(new Track{Name=title,Channel=channel,Notes=notes});
        }
        Validate(p);return p;
    }
    public static string Doctor(string file){
        using var fs=File.OpenRead(file);using var r=new BinaryReader(fs);
        if(Encoding.ASCII.GetString(r.ReadBytes(4))!="FLhd")throw new InvalidDataException("Nie rozpoznano nagłówka FLP.");
        int headerLength=Read32(r);
        if(headerLength<6||headerLength>128)throw new InvalidDataException("Uszkodzony nagłówek FLP.");
        int fmt=Read16(r),channels=Read16(r),ppq=Read16(r);
        return $"Jerzy FL Project Doctor (tylko odczyt)\r\nProjekt: {Path.GetFileName(file)}\r\nRozmiar: {fs.Length:N0} B\r\nFormat: {fmt}\r\nKanały: {channels}\r\nPPQ: {ppq}\r\n\r\nAnaliza podstawowa: nie odczytuje pełnych patternów, pluginów ani aranżacji.\r\nPlik projektu nie został zmieniony.";
    }
}