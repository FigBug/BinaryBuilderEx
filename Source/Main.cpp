/*
  ==============================================================================

   Utility to turn a bunch of binary files into a .cpp file and .h file full of
   data so they can be built directly into an executable.

   Use this code at your own risk! It carries no warranty!

  ==============================================================================
*/

#include "../JuceLibraryCode/JuceHeader.h"


//==============================================================================
String generateName(const File& file, const File& sourceDir, bool addpath)
{
    String name (file.getFileName()
                   .replaceCharacter (' ', '_')
                   .replaceCharacter ('.', '_')
                   .replaceCharacter ('/', '_')
                   .replaceCharacter ('\\', '_')
                   .retainCharacters ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789"));

    if (addpath)
    {
        String path = file.getParentDirectory().getRelativePathFrom (sourceDir);
        if (!path.startsWith(".."))
        {
            path = path.replaceCharacter (' ', '_')
                    .replaceCharacter ('.', '_')
                    .replaceCharacter ('/', '_')
                    .replaceCharacter ('\\', '_')
                    .retainCharacters ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789");
            
            name = path + "_" + name;
        }
    }
    
    if (isdigit(name[0]))
        name = "_" + name;
    
    return name;
}

static int addFile (const File& file,
                    const File& source,
                    bool addDir,
                    const String& classname,
                    OutputStream& headerStream,
                    OutputStream& cppStream)
{
    MemoryBlock mb;
    file.loadFileAsData (mb);

    const String name = generateName(file, source, addDir);

    std::cout << "Adding " << name << ": "
              << (int) mb.getSize() << " bytes" << std::endl;

    headerStream << "    extern const char*  " << name << ";\r\n"
                    "    const int           " << name << "Size = "
                 << (int) mb.getSize() << ";\r\n\r\n";

    static int tempNum = 0;

    cppStream << "static const unsigned char temp" << ++tempNum << "[] = {";

    size_t i = 0;
    const uint8* const data = (const uint8*) mb.getData();

    while (i < mb.getSize() - 1)
    {
        if ((i % 40) != 39)
            cppStream << (int) data[i] << ",";
        else
            cppStream << (int) data[i] << ",\r\n  ";

        ++i;
    }

    cppStream << (int) data[i] << ",0,0};\r\n";

    cppStream << "const char* " << classname << "::" << name
              << " = (const char*) temp" << tempNum << ";\r\n\r\n";

    return (int) mb.getSize();
}

static bool isHiddenFile (const File& f, const File& root)
{
    return f.getFileName().endsWithIgnoreCase (".scc")
         || f.getFileName() == ".svn"
         || f.getFileName().startsWithChar ('.')
         || (f.getSize() == 0 && ! f.isDirectory())
         || (f.getParentDirectory() != root && isHiddenFile (f.getParentDirectory(), root));
}


//==============================================================================
int main (int argc, char* argv[])
{
    std::cout << std::endl << " BinaryBuilder!  Visit www.juce.com for more info." << std::endl;
    
    bool addpath = false;
    
    StringArray args;
    for (int i = 0; i < argc; i++)
    {
        String arg = argv[i];
        if (arg.startsWith("-"))
        {
            if (arg == "-addpath")
                addpath = true;
        }
        else
        {
            args.add(arg);
        }
    }
    
    if (args.size() < 5 || args.size() > 6)
    {
        std::cout << " Usage: BinaryBuilder <-addpath> minfilestocreate sourcedirectory targetdirectory targetclassname [optional wildcard pattern]\n\n"
        " BinaryBuilder will find all files in the source directory, and encode them\n"
        " into two files called (targetclassname).cpp and (targetclassname).h, which it\n"
        " will write into the target directory supplied.\n\n"
        " Any files in sub-directories of the source directory will be put into the\n"
        " resultant class, but #ifdef'ed out using the name of the sub-directory (hard to\n"
        " explain, but obvious when you try it...)\n";
        
        return 0;
    }
    
    
    const File sourceDirectory (File::getCurrentWorkingDirectory()
                                     .getChildFile (args[2].unquoted()));

    if (! sourceDirectory.isDirectory())
    {
        std::cout << "Source directory doesn't exist: "
                  << sourceDirectory.getFullPathName()
                  << std::endl << std::endl;

        return 0;
    }

    const File destDirectory (File::getCurrentWorkingDirectory()
                                   .getChildFile (args[3].unquoted()));

    if (! destDirectory.isDirectory())
    {
        std::cout << "Destination directory doesn't exist: "
                  << destDirectory.getFullPathName() << std::endl << std::endl;

        return 0;
    }

    String className (args[4]);
    className = className.trim();
    
    int currentFile = 1;
    
    Array<File> files;
    sourceDirectory.findChildFiles (files, File::findFiles, true,
                                    (args.size() > 5) ? args[5] : "*");

    const File headerFile (destDirectory.getChildFile (className).withFileExtension (".h"));
    File cppFile (destDirectory.getChildFile (className + String (currentFile)).withFileExtension (".cpp"));
    File cppWrapperFile (destDirectory.getChildFile (className + String (currentFile) + "Wrapper").withFileExtension (".cpp"));
    
    if (files.size() == 0)
    {
        std::cout << "Didn't find any source files in: "
                  << sourceDirectory.getFullPathName() << std::endl << std::endl;
        return 0;
    }
    
    bool needsProcessing = false;
    if (! headerFile.existsAsFile())
    {
        needsProcessing = true;
    }
    else
    {
        for (auto f : files)
        {
            if (f.getLastModificationTime() > headerFile.getLastModificationTime())
            {
                needsProcessing = true;
                break;
            }
        }
    }
    
    if (! needsProcessing)
    {
        std::cout << "No processing required. No files updated" << std::endl;
        return 0;
    }
    
    std::cout << "Creating " << headerFile.getFullPathName()
              << " and " << cppFile.getFullPathName()
              << " from files in " << sourceDirectory.getFullPathName()
              << "..." << std::endl << std::endl;


    headerFile.deleteFile();
    
    for (int i = 0; i < 1000; i++)
    {
        const File oldCppFile (destDirectory.getChildFile (className + String (i)).withFileExtension (".cpp"));
        const File oldCppWrapperFile (destDirectory.getChildFile (className + String (i) + "Wrapper").withFileExtension (".cpp"));
        
        oldCppFile.deleteFile();
        oldCppWrapperFile.deleteFile();
    }
    
    auto header = headerFile.createOutputStream();

    if (header == nullptr)
    {
        std::cout << "Couldn't open "
                  << headerFile.getFullPathName() << " for writing" << std::endl << std::endl;
        return 0;
    }

    auto cpp = cppFile.createOutputStream();

    if (cpp == nullptr)
    {
        std::cout << "Couldn't open "
                  << cppFile.getFullPathName() << " for writing" << std::endl << std::endl;
        return 0;
    }
    
    *header << "/* (Auto-generated binary data file). */\r\n\r\n"
               "#ifndef BINARY_" << className.toUpperCase() << "_H\r\n"
               "#define BINARY_" << className.toUpperCase() << "_H\r\n\r\n"
               "namespace " << className << "\r\n"
               "{\r\n\r\n";
    
    *header << "    struct Info { const char* name; const char* path; const char* data; int size; };\r\n";
    *header << "    extern Info info[];\r\n";
    *header << "    extern int infoSize;\r\n\r\n";
    
    *cpp << "/* (Auto-generated binary data file). */\r\n\r\n"
            "#include \"" << className << ".h\"\r\n\r\n";
    
    cppWrapperFile.replaceWithText("#include \"" + className + String (currentFile) + ".cpp\"\n");

    int totalBytes = 0;
    int currBytes = 0;
    
    Array<int> bytes;

    for (int i = 0; i < files.size(); ++i)
    {
        const File file (files[i]);

        // (avoid source control files and hidden files..)
        if (! isHiddenFile (file, sourceDirectory))
        {
            int sz = addFile (file, sourceDirectory, addpath, className, *header, *cpp);
            bytes.add (sz);
            totalBytes += sz;
            currBytes += sz;
        }
        
        cpp->flush();
        if (cppFile.getSize() >= 15 * 1024 * 1024)
        {
            cppFile = File (destDirectory.getChildFile (className + String (++currentFile)).withFileExtension (".cpp"));
            cppWrapperFile = File (destDirectory.getChildFile (className + String (currentFile) + "Wrapper").withFileExtension (".cpp"));
            
            cppWrapperFile.replaceWithText ("#include \"" + className + String (currentFile) + ".cpp\"\n");
            
            cpp = cppFile.createOutputStream();
            
            if (cpp == nullptr)
            {
                std::cout << "Couldn't open "
                          << cppFile.getFullPathName() << " for writing" << std::endl << std::endl;
                return 0;
            }
            
            *cpp << "/* (Auto-generated binary data file). */\r\n\r\n"
                    "#include \"" << className << ".h\"\r\n\r\n";

            currBytes = 0;
        }
    }
    int cnt = 0;
    *cpp << className << "::Info " << className << "::info[]  = {\r\n";
    for (int i = 0; i < files.size(); ++i)
    {
        const File file (files[i]);
        
        // (avoid source control files and hidden files..)
        if (! isHiddenFile (file, sourceDirectory))
        {
            String name = "\"" + file.getFileName() + "\"";
            String data = className + "::" + generateName(file, sourceDirectory, addpath);
            String path = "\"" + file.getParentDirectory().getRelativePathFrom (sourceDirectory) + "\"";
            if (path.startsWith("\"..")) path = "\"\"";
            String size = String (bytes[cnt++]);
            *cpp << "    { " << name << ", " << path << ", " << data << ", " << size << " },\r\n";
        }
        
    }
    *cpp << "};\r\n\n";
    *cpp << "int " << className << "::infoSize = " << String(cnt) << ";\r\n";

    *header << "}\r\n\r\n"
               "#endif\r\n";

    header = nullptr;
    cpp = nullptr;

    std::cout << std::endl << " Total size of binary data: " << totalBytes << " bytes" << std::endl;
    
    // Create some more dummy wrapper
    currentFile++;
    while (currentFile <= args[1].getIntValue())
    {
        std::cout << std::endl << " Create empty wrapper file for " << args[1] << std::endl;
        
        cppWrapperFile = File (destDirectory.getChildFile (className + String (currentFile) + "Wrapper").withFileExtension (".cpp"));
        cppWrapperFile.replaceWithText ("\n");
        
        currentFile++;
    }

    return 0;
}
