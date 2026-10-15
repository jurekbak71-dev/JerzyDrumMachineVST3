using System.Diagnostics;
using System.Drawing;
using System.Reflection;
using System.Text;
using System.Windows.Forms;

namespace JerzyFLBridge;

internal static class Program {
    [STAThread]
    private static void Main() {
        ApplicationConfiguration.Initialize();
        Application.Run(new BridgeWindow());
    }
}

internal sealed class BridgeWindow : Form {
    readonly TabControl tabs=new(){Dock=DockStyle.Fill};
    readonly Label status=new(){Text="Gotowy. Wybierz funkcję, a następnie kliknij przycisk.",Dock=DockStyle.Bottom,Height=36,ForeColor=Color.FromArgb(35,72,68),Padding=new Padding(14,8,8,4)};
    static readonly Color Back=Color.FromArgb(245,246,247),Primary=Color.FromArgb(28,78,77);
    readonly Font bodyFont=new("Segoe UI",10);
    public BridgeWindow() {
        Text="Jerzy FL Studio Bridge 0.2  —  bez Pythona";
        Width=950;Height=700;MinimumSize=new Size(850,620);
        BackColor=Back;Font=bodyFont;StartPosition=FormStartPosition.CenterScreen;
        var header=new Panel{Dock=DockStyle.Top,Height=90,BackColor=Primary};
        header.Controls.Add(new Label{Text="JERZY  /  FL STUDIO BRIDGE",Font=new Font("Segoe UI Semibold",19),ForeColor=Color.White,Left=22,Top=15,Width=800,Height=33});
        header.Controls.Add(new Label{Text="MIDI · GROOVE · BASS · PIANO ROLL · PROJEKTY FLP",Font=new Font("Segoe UI",10),ForeColor=Color.FromArgb(212,230,227),Left=24,Top=55,Width=800,Height=22});
        Controls.Add(tabs);Controls.Add(status);Controls.Add(header);
        tabs.Padding=new Point(16,9);
        NewTab("Perkusja",DrumTab);
        NewTab("Bas i akordy",BassTab);
        NewTab("Polirytmia",EuclideanTab);
        NewTab("Pliki MIDI / JPF",FileTab);
        NewTab("FL Studio",FLTab);
        NewTab("Diagnostyka FLP",DoctorTab);
    }
    static Label TextLabel(string s,int x,int y,int w=350,int h=24) =>
        new(){Text=s,Left=x,Top=y,Width=w,Height=h,ForeColor=Color.FromArgb(35,45,48)};
    static Button Action(string s,int x,int y,int w=310) =>
        new(){Text=s,Left=x,Top=y,Width=w,Height=44,BackColor=Primary,ForeColor=Color.White,FlatStyle=FlatStyle.Flat,UseVisualStyleBackColor=false};
    static NumericUpDown Num(int x,int y,int min,int max,int value,int w=120) =>
        new(){Left=x,Top=y,Width=w,Minimum=min,Maximum=max,Value=value,ThousandsSeparator=false};
    static TextBox Input(string s,int x,int y,int w) =>
        new(){Text=s,Left=x,Top=y,Width=w,Height=30};
    static Label Expl(string s,int y) =>
        new(){Text=s,Left=24,Top=y,Width=820,Height=90,ForeColor=Color.FromArgb(85,95,97),Font=new Font("Segoe UI",9.5f)};
    void NewTab(string title,Action<Panel> builder){
        var page=new TabPage(title){BackColor=Back,Padding=new Padding(0)};
        var scroll=new Panel{Dock=DockStyle.Fill,AutoScroll=true,BackColor=Back};
        page.Controls.Add(scroll);tabs.TabPages.Add(page);builder(scroll);
    }
    void Execute(Action job,string success){
        try{Cursor=Cursors.WaitCursor;job();status.Text=success;MessageBox.Show(this,success,"Jerzy FL Studio Bridge",MessageBoxButtons.OK,MessageBoxIcon.Information);}
        catch(Exception e){status.Text="Błąd: "+e.Message;MessageBox.Show(this,e.Message,"Nie udało się",MessageBoxButtons.OK,MessageBoxIcon.Error);}
        finally{Cursor=Cursors.Default;}
    }
    string? Save(string filter,string defaultName){
        using var dlg=new SaveFileDialog{Filter=filter,FileName=defaultName,AddExtension=true,OverwritePrompt=true};
        return dlg.ShowDialog(this)==DialogResult.OK?dlg.FileName:null;
    }
    string? Open(string filter){
        using var dlg=new OpenFileDialog{Filter=filter,CheckFileExists=true};
        return dlg.ShowDialog(this)==DialogResult.OK?dlg.FileName:null;
    }
    void SavePatternAndMidi(Pattern p,string defaultName){
        var file=Save("Plik MIDI (*.mid)|*.mid",""+defaultName+".mid");if(file is null)return;
        Execute(()=>{
            Music.ToMidi(p,file);
            Music.Save(p,Path.ChangeExtension(file,"jpf.json"));
        },"Zapisano MIDI oraz JPF obok siebie:\n"+file);
    }
    void DrumTab(Panel p){
        p.Controls.Add(TextLabel("Tempo (BPM)",24,30));
        var bpm=Num(250,28,20,400,120);p.Controls.Add(bpm);
        p.Controls.Add(TextLabel("Liczba taktów",24,80));
        var bars=Num(250,78,1,32,4);p.Controls.Add(bars);
        p.Controls.Add(TextLabel("Wariant rytmiczny (seed)",24,130));
        var seed=Num(250,128,0,999999,42);p.Controls.Add(seed);
        var trip=new CheckBox{Left=24,Top=186,Width=560,Height=32,Text="Hi-hat: niezależne triole szesnastkowe",Checked=false};
        p.Controls.Add(trip);
        p.Controls.Add(TextLabel("Swing hi-hatu (%)",24,239));
        var swing=Num(250,237,0,49,18);p.Controls.Add(swing);
        var generate=Action("GENERUJ I ZAPISZ MIDI",24,306,365);p.Controls.Add(generate);
        generate.Click+=(_,_)=>SavePatternAndMidi(Music.Drums((double)bpm.Value,(int)bars.Value,(int)seed.Value,trip.Checked,(double)swing.Value/100.0),"Jerzy_Drums");
        p.Controls.Add(Expl("Zapisuje plik MIDI do przeciągnięcia do Piano Roll oraz drugi plik .jpf.json.\nKażdy instrument ma osobną ścieżkę MIDI. Nuty perkusji 36–47 współpracują z Drum Machine.",385));
    }
    void BassTab(Panel p){
        p.Controls.Add(TextLabel("Progresja akordów, oddzielona przecinkami",24,30,700));
        var chords=Input("Am,F,C,G",24,62,660);p.Controls.Add(chords);
        p.Controls.Add(TextLabel("Styl",24,128));
        var style=new ComboBox{Left=250,Top=125,Width=180,DropDownStyle=ComboBoxStyle.DropDownList};
        style.Items.AddRange(new object[]{"Rock","Dub","Acid","Funk","Ambient"});style.SelectedIndex=0;p.Controls.Add(style);
        p.Controls.Add(TextLabel("Tempo (BPM)",24,182));
        var bpm=Num(250,180,20,400,120);p.Controls.Add(bpm);
        var generate=Action("GENERUJ BAS I AKORDY MIDI",24,255,365);p.Controls.Add(generate);
        generate.Click+=(_,_)=>SavePatternAndMidi(Music.Bass((double)bpm.Value,chords.Text,style.Text),"Jerzy_Bass");
        p.Controls.Add(Expl("Powstają dwie ścieżki: BASS oraz CHORD GUIDE. Zaimportuj MIDI do FL Studio.\nW Bass Ambient wybierz DIRECT, aby instrument odtwarzał dokładnie te nuty.",338));
    }
    void EuclideanTab(Panel p){
        p.Controls.Add(TextLabel("Ścieżki: nazwa : liczba uderzeń : liczba kroków : obrót",24,30,800));
        var lanes=Input("KICK:5:16:0,CH:11:24:2",24,65,720);p.Controls.Add(lanes);
        p.Controls.Add(TextLabel("Tempo (BPM)",24,129));
        var bpm=Num(250,126,20,400,127);p.Controls.Add(bpm);
        p.Controls.Add(TextLabel("Liczba taktów",24,178));
        var bars=Num(250,175,1,32,4);p.Controls.Add(bars);
        var generate=Action("GENERUJ POLIRYTMIĘ MIDI",24,245,365);p.Controls.Add(generate);
        generate.Click+=(_,_)=>SavePatternAndMidi(Music.Euclidean((double)bpm.Value,lanes.Text,(int)bars.Value),"Jerzy_Euclidean");
        p.Controls.Add(Expl("W każdej ścieżce można ustawić inną liczbę kroków w takcie.\nPrzykład: KICK 5 uderzeń / 16 kroków, CH 11 uderzeń / 24 kroki.",325));
    }
    void FileTab(Panel p){
        var j2m=Action("ZAMIENIAM JPF → MIDI",24,35,350);p.Controls.Add(j2m);
        j2m.Click+=(_,_)=>{
            var source=Open("Jerzy Pattern (*.jpf.json;*.json)|*.jpf.json;*.json");if(source is null)return;
            var dest=Save("MIDI (*.mid)|*.mid",Path.GetFileNameWithoutExtension(source)+".mid");if(dest is null)return;
            Execute(()=>Music.ToMidi(Music.Load(source),dest),"Zapisano plik MIDI: "+dest);
        };
        var m2j=Action("ZAMIENIAM MIDI → JPF",24,100,350);p.Controls.Add(m2j);
        m2j.Click+=(_,_)=>{
            var source=Open("MIDI (*.mid;*.midi)|*.mid;*.midi");if(source is null)return;
            var dest=Save("Jerzy Pattern (*.jpf.json)|*.jpf.json",Path.GetFileNameWithoutExtension(source)+".jpf.json");if(dest is null)return;
            Execute(()=>Music.Save(Music.FromMidi(source),dest),"Zapisano plik JPF: "+dest);
        };
        p.Controls.Add(TextLabel("Arpeggiator z akordów w pliku JPF",24,190,700));
        p.Controls.Add(TextLabel("Podział",24,232));
        var division=new ComboBox{Left=250,Top=229,Width=155,DropDownStyle=ComboBoxStyle.DropDownList};
        division.Items.AddRange(new object[]{"1/8","1/8T","1/16","1/16T","1/32"});division.SelectedIndex=2;p.Controls.Add(division);
        p.Controls.Add(TextLabel("Kierunek",24,274));
        var direction=new ComboBox{Left=250,Top=271,Width=155,DropDownStyle=ComboBoxStyle.DropDownList};
        direction.Items.AddRange(new object[]{"Up","Down","Up-Down"});direction.SelectedIndex=0;p.Controls.Add(direction);
        var arp=Action("UTWÓRZ ARPEGGIO MIDI",24,332,350);p.Controls.Add(arp);
        arp.Click+=(_,_)=>{
            var file=Open("Jerzy Pattern (*.jpf.json;*.json)|*.jpf.json;*.json");if(file is null)return;
            var destination=Save("Plik MIDI (*.mid)|*.mid","Jerzy_Arp.mid");if(destination is null)return;
            Execute(()=>{
                var result=Music.Arp(Music.Load(file),new[]{2,3,4,6,8}[division.SelectedIndex],1,direction.Text);
                Music.ToMidi(result,destination);Music.Save(result,Path.ChangeExtension(destination,"jpf.json"));
            },"Zapisano arpeggio: "+destination);
        };
        p.Controls.Add(Expl("Nie trzeba znać Pythona. Gotowy plik .mid przeciągasz myszą do FL Studio.\nŹródłowy plik JPF można otworzyć ponownie i zmienić jego strukturę przez skrypty Piano Roll.",415));
    }
    void FLTab(Panel p){
        p.Controls.Add(TextLabel("Instalacja dodatkowych skryptów Piano Roll",24,30,720));
        p.Controls.Add(Expl("Kliknij INSTALUJ. Program sam skopiuje cztery skrypty do folderu użytkownika FL Studio.\nNie wymaga instalacji zewnętrznego Pythona — skrypty uruchamia sam FL Studio.",75));
        var dest=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),"Image-Line","FL Studio","Settings","Piano roll scripts","Jerzy");
        var folder=Input(dest,24,168,710);p.Controls.Add(folder);
        var select=Action("WYBIERZ INNY FOLDER",24,218,260);p.Controls.Add(select);
        select.Click+=(_,_)=>{using var f=new FolderBrowserDialog{Description="Wybierz folder Jerzy w Piano roll scripts",SelectedPath=Directory.Exists(folder.Text)?folder.Text:Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments)};
            if(f.ShowDialog(this)==DialogResult.OK)folder.Text=f.SelectedPath;};
        var install=Action("INSTALUJ SKRYPTY FL STUDIO",24,290,390);p.Controls.Add(install);
        install.Click+=(_,_)=>Execute(()=>{
            if(string.IsNullOrWhiteSpace(folder.Text))throw new InvalidDataException("Brak katalogu docelowego.");
            Directory.CreateDirectory(folder.Text);
            var asm=Assembly.GetExecutingAssembly();
            var resourceNames=asm.GetManifestResourceNames().Where(x=>x.StartsWith("Scripts.")&&x.EndsWith(".pyscript")).ToArray();
            if(resourceNames.Length!=4)throw new InvalidOperationException("W aplikacji brakuje skryptów Piano Roll.");
            foreach(var name in resourceNames){
                using var input=asm.GetManifestResourceStream(name)??throw new FileNotFoundException(name);
                using var output=File.Create(Path.Combine(folder.Text,name["Scripts.".Length..]));
                input.CopyTo(output);
            }
        },"Zainstalowano 4 skrypty. Uruchom ponownie FL Studio i otwórz Piano Roll → Tools → Scripts.");
        var open=Action("OTWÓRZ FOLDER SKRYPTÓW",24,355,320);p.Controls.Add(open);
        open.Click+=(_,_)=>Execute(()=>{Directory.CreateDirectory(folder.Text);Process.Start(new ProcessStartInfo("explorer.exe",'"'+folder.Text+'"'){UseShellExecute=true});},"Otwarto folder skryptów.");
        p.Controls.Add(Expl("Dostępne: Jerzy Groove Triplets, Jerzy Arp Chords, Jerzy Import JPF, Jerzy Export JPF.\nJeżeli masz niestandardowy folder danych FL Studio, wybierz go przyciskiem powyżej.",445));
    }
    void DoctorTab(Panel p){
        p.Controls.Add(TextLabel("Podstawowa analiza nagłówka projektu FL Studio (.flp)",24,30,750));
        var output=new TextBox{Left=24,Top=120,Width=780,Height=270,Multiline=true,ReadOnly=true,ScrollBars=ScrollBars.Vertical,Font=new Font("Consolas",10),BackColor=Color.White};
        p.Controls.Add(output);
        var button=Action("WYBIERZ PROJEKT FLP",24,65,330);p.Controls.Add(button);
        button.Click+=(_,_)=>{
            var file=Open("FL Studio Project (*.flp)|*.flp");if(file is null)return;
            Execute(()=>{output.Text=Music.Doctor(file);},"Odczytano nagłówek projektu. Plik FLP pozostał bez zmian.");
        };
        var save=Action("ZAPISZ RAPORT TXT",24,420,310);p.Controls.Add(save);
        save.Click+=(_,_)=>{
            if(output.Text.Length==0){MessageBox.Show(this,"Najpierw otwórz projekt FLP.");return;}
            var path=Save("Tekst (*.txt)|*.txt","Jerzy_FL_Project_Doctor.txt");if(path is null)return;
            Execute(()=>File.WriteAllText(path,output.Text,Encoding.UTF8),"Zapisano raport: "+path);
        };
        p.Controls.Add(Expl("Bezpieczny podgląd wyłącznie do odczytu: typ pliku, liczba kanałów, PPQ, rozmiar.\nTo wersja podstawowa — nie analizuje pełnej zawartości projektów FLP.",490));
    }
}