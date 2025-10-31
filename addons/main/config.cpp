class CfgPatches {
	class wtaPlugin { //Change this
		name = "wtaPlugin"; //Change this
		units[] = {};
		weapons[] = {};
		requiredVersion = 1.82;		
		requiredAddons[] = {"intercept_core"};
		author = "ruko"; //Change this
		authors[] = {"ruko"}; //Change this
		url = "https://github.com/intercept/intercept-plugin-template"; //Change this
		version = "1.0";
		versionStr = "1.0";
		versionAr[] = {1,0};
	};
};
class Intercept {
    class ruko { //Change this. It's either the name of your project if you have more than one plugin. Or just your name.
        class wtaPlugin { //Change this.
            pluginName = "wtaPlugin"; //Change this.
        };
    };
};
