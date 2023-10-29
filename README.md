<p align="center">
  <img width="128" height="128" src="images/logo.png">
</p>

# Introduction
[Notion Blog(한국어)](https://cuboid-tarantula-e0b.notion.site/Real-Time-Heat-Diffusion-4027725cc9e547aea4269ecee8bc0f40?pvs=4)

# Installation
1. Go to your project folder
2. Create a folder `Plugins` (if not exist)
3. Copy and paste `Heatbox` folder under `Plugins`

# Project Settings

![alt_text](images/CollisionSettings.png "Collision Settings")

Alternatively you can add this lines of code to `DefaultEngine` under `Config` folder.
```
+DefaultChannelResponses=(Channel=ECC_GameTraceChannel1,DefaultResponse=ECR_Block,bTraceType=False,bStaticObject=False,Name="Flammable")
+DefaultChannelResponses=(Channel=ECC_GameTraceChannel2,DefaultResponse=ECR_Block,bTraceType=False,bStaticObject=False,Name="Burning")
```
***Note that the ordering does matter!***

# Example
You can test the plugin's features with the example map I set up for you and the basic setup can be found on its level blueprint.

![alt_text](images/ExampleMap.png "Example Map")

You may hit the play right away to get a feel for it.

Or, play with `DT_FireSim_BoxInfo`. This data table contains core variables that affect the simulation.

![alt_text](images/DataTable.png "Data Table")

```
[열 전달 방정식]
온도 변화량 = 열 에너지 x 열 흡수율
void ReceiveHeat(float HeatEnergy, float UpdateInterval);
 
[흑체(Black-body) 방정식]
열 에너지 = 열 방출율 x 슈테판-볼츠만 상수 x (최대 온도[K]^4 - (최대 온도 - 현재 온도)[K]^4) x 방열 면적[m^2]
float RadiateHeat();

[Core variables]
MaxTemperature: The maximum temperature that a substance undergoing a combustion reaction can reach
IgnitionPoint: The temperature at which the combustion reaction of a substance starts
HeatAbsorbRate: Inherent heat energy absorption efficiency of a material
HeatEmitRate: Inherent heat energy release efficiency of a material
FuelCount: Remaining fuel or combustion rate of the material in which the combustion reaction took place
RadiationArea: The surface area of ​​the material where the combustion reaction took place. This parameter is applied to the black-body equation that calculates the amount of heat that causes damage or increases the temperature of nearby players and objects.
MinFireSize: Adjust the minimum size of the effect to visualize the combustion reaction
MaxFireSize: Adjust the maximum size of the effect visualizing the combustion reaction
HeatDamageApplied: The dammage applied to environemnt
HeatDamageReceived: The damate applied to an object
IgnitionCore: The point of a substance in which a combustion reaction has taken place
```
