
using Std::React::@reactive;

public fun buildReactiveUI() -> CCNode {
    CCNode {
        @reactive
        let count: int = 0;
        
        layout: ColumnLayout {}
        CCLabelBMFont {
            text: $"Clicked {count} times",
        }
        CCMenu {
            CCMenuItemSpriteExtra {
                ButtonSprite {
                    text: "Click me!",
                }
                clicked: () => {
                    count += 1;
                }
            }
        }
    }
}
