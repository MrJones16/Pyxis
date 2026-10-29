# Pyxis
Pyxis Game Engine

## About
Pyxis is a NO-AI open-source game engine written in C++ focused on advanced falling sand simulation features & lockstep multiplayer networking.
My goal with the engine is to provide a platform where other developers can make games like mine, which is inspired by The Powder Toy and Noita.

During development, I struggled with the decision on whether or not I should use AI to assist in the development of this project.
I have tried it, and modern models are absolutely phenomenal. However, after heavy consideration, I've decided that there will be no
AI code in this project. This is because even if I write 90% of the code, I fear that any AI usage puts you in the "slop" category,
and that's not something I want for this project. It also lets me say that yes, I wrote every line, and implemented everything myself.

For future projects, I will likely dedicate sections of the code to strictly AI, and strictly human. I found this concept from ThingsPool's "Shards of Sanity" blog: https://thingspool.net/shards-of-sanity/list.html
On this project, because I have been working on variations of this idea and learning all the concepts for like 3 years, I don't want to tarnish what I've built, and I want to 
go the rest of the way myself!

## Libraries/Packages/Frameworks
- SDL3
- GameNetworkingSockets
- Box2D
- Nlohmann Json
- Snappy

## Credits
David Capello for the Aseprite Font! [Github Link](https://github.com/aseprite/aseprite/tree/main/data/fonts) I converted it to a TTF.

## More Details
This is V2 of the engine, and this version will be based on SDL3 and an ECS. 

The prior was my first attempt and it was from scratch following some tutorials, and while I learned a lot, it was not the best implementation and it racked up too much tech debt. It was also very inheritance based which is not terrible but I want to go towards an actual ECS kinda engine, for the people!
I also wanted to go with SDL3 because it will be much more widespread and usable compared to all the rendering being purely in my own API. While the engine will have rendering API, You could still just write your own stuff using the SDL3.


