use image::{GenericImageView, ImageReader};

const DRAW_FUNC: &'static str = "tft.drawPixel";
const RGB_FUNC: &'static str = "~RGB_TO_HEX";

fn main() {
    let x_offset = 205;
    let y_offset = 125;
    let x_size = 115;
    let y_size = 115;

    let img = ImageReader::open("mars.bmp").unwrap().decode().unwrap();

    for x in 0..x_size {
        for y in 0..y_size {
            let pixel = img.get_pixel(x, y);
            let x = x + x_offset;
            let y = y + y_offset;
            println!(
                "{DRAW_FUNC}({x}, {y}, {RGB_FUNC}({}, {}, {}));",
                pixel.0[0], pixel.0[1], pixel.0[2]
            );
        }
    }
}
