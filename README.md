# UE4 Enhanced Output Log

This repository contains the source code for the following UE4 plugin: [UE4 marketplace](https://www.unrealengine.com/marketplace/enhanced-output-log)

# How to compile and install

1. Clone the repository
2. Install UE4
3. open the file `compile.bat` and adjust the file paths in there:
 * `RunUAT.bat` has to point to our UE4 installation
 * The `Plugin` parameter has to point to the the `ConsoleEnhanced.uplugin ` file in this repository
 * The `Package`parameter is the output folder
4. run the `compile.bat` file and check the output folder for the plugin
5. drop the output folder either in your project's or the engine's "Plugins" folder

# How to contribute

You have a cool idea or found a bug? Cool, fork the project, commit your changes and open a pull request :)

# License

1. You have to accept the UE4 license before using this
2. You are free to share, modify and use the plugin as you wish (even commercially)
